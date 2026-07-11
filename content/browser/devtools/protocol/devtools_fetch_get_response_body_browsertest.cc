// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

// Regression (TimeBack / Electron Crashpad 8840e25c):
// Fetch.enable (Response) + Fetch.getResponseBody → client_receiver_.Resume()
// can null/UAF MultiplexRouter when a queued OnComplete resets the receiver
// while the job remains gettable (kResponseReceived + waiting).
//
// Uses GET (no request_body) so NotifyClient does not enter RequestBodyCollector
// (that is a separate UAF fixed by the failed-POST WeakPtr patch).
class DevToolsFetchGetResponseBodyTest : public DevToolsProtocolTest {
 public:
  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override {
    if (!auto_handle_paused_ || in_auto_handle_) {
      DevToolsProtocolTest::DispatchProtocolMessage(agent_host, message);
      return;
    }

    std::optional<base::Value> parsed = base::JSONReader::Read(
        std::string_view(reinterpret_cast<const char*>(message.data()),
                         message.size()),
        base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    if (parsed && parsed->is_dict()) {
      const std::string* method = parsed->GetDict().FindString("method");
      if (method && *method == "Fetch.requestPaused") {
        const base::DictValue* params = parsed->GetDict().FindDict("params");
        const std::string* request_id =
            params ? params->FindString("requestId") : nullptr;
        if (request_id) {
          in_auto_handle_ = true;
          // TimeBack order: body first, then continue.
          base::DictValue body_params;
          body_params.Set("requestId", *request_id);
          SendCommandSync("Fetch.getResponseBody", std::move(body_params));
          got_body_++;

          base::DictValue cont;
          cont.Set("requestId", *request_id);
          SendCommandSync("Fetch.continueResponse", std::move(cont));
          continued_++;
          in_auto_handle_ = false;
        }
      }
    }

    DevToolsProtocolTest::DispatchProtocolMessage(agent_host, message);
  }

 protected:
  bool auto_handle_paused_ = false;
  bool in_auto_handle_ = false;
  int got_body_ = 0;
  int continued_ = 0;
};

IN_PROC_BROWSER_TEST_F(DevToolsFetchGetResponseBodyTest,
                       ResponseStageGetResponseBodyDoesNotNullResume) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL page_url = embedded_test_server()->GetURL("/title1.html");
  // Small cacheable GETs: OnReceiveResponse + OnComplete often queued before
  // Pause(); GetResponseBody → Resume() then runs OnComplete which
  // client_receiver_.reset()s underfoot.
  GURL get_url = embedded_test_server()->GetURL("/echo");
  NavigateToURLBlockUntilNavigationsComplete(shell(), page_url, 1);

  Attach();

  base::DictValue enable_params;
  base::ListValue patterns;
  {
    base::DictValue p;
    p.Set("requestStage", "Response");
    p.Set("resourceType", "Fetch");
    patterns.Append(std::move(p));
  }
  {
    base::DictValue p;
    p.Set("requestStage", "Response");
    p.Set("resourceType", "XHR");
    patterns.Append(std::move(p));
  }
  enable_params.Set("patterns", std::move(patterns));
  SendCommandSync("Fetch.enable", std::move(enable_params));
  ASSERT_FALSE(error());

  auto_handle_paused_ = true;

  std::string script = base::StringPrintf(
      R"((async () => {
        const tasks = [];
        for (let i = 0; i < 40; i++) {
          tasks.push(
              fetch('%s?n=' + i, {credentials: 'omit'})
                  .then(r => r.text())
                  .catch(() => null));
        }
        await Promise.allSettled(tasks);
        return 'done';
      })())",
      get_url.spec().c_str());

  content::ExecuteScriptAsync(shell()->web_contents(), script);

  base::TimeTicks deadline = base::TimeTicks::Now() + base::Seconds(60);
  while (base::TimeTicks::Now() < deadline && got_body_ < 5) {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(50));
    run_loop.Run();
  }

  auto_handle_paused_ = false;

  // Under ASAN/debug + bug: DCHECK(router_)/null Resume / UAF in
  // MultiplexRouter while handling Fetch.getResponseBody.
  // Must NOT be RequestBodyCollector (no POST body on this test).
  EXPECT_GE(got_body_, 1);
  EXPECT_GE(continued_, 1);
  SendCommandSync("Fetch.disable");
}

}  // namespace content
