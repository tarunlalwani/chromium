// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/run_loop.h"
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

// Regression: Fetch.enable (Response) + failed POST *with a body* must not UAF
// InterceptionJob. NotifyClient may Collect bodies with base::Unretained(this);
// a synchronous Fetch.continueResponse from RequestPaused can Shutdown/delete
// the job before Collect returns (Electron/TimeBack Datadog-shaped crash).
class DevToolsFetchFailedRequestsTest : public DevToolsProtocolTest {
 public:
  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override {
    if (!auto_continue_paused_ || in_auto_continue_) {
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
          in_auto_continue_ = true;
          base::DictValue cont;
          cont.Set("requestId", *request_id);
          SendCommandSync("Fetch.continueResponse", std::move(cont));
          continued_++;
          in_auto_continue_ = false;
        }
      }
    }

    DevToolsProtocolTest::DispatchProtocolMessage(agent_host, message);
  }

 protected:
  bool auto_continue_paused_ = false;
  bool in_auto_continue_ = false;
  int continued_ = 0;
};

IN_PROC_BROWSER_TEST_F(DevToolsFetchFailedRequestsTest,
                       ResponseStageFailedPostWithBodyDoesNotUseAfterFree) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL page_url = embedded_test_server()->GetURL("/title1.html");
  // Capture a same-origin POST URL, then shut the server down so the POST
  // fails with connection refused *without* a CORS preflight (OPTIONS has no
  // body and does not exercise RequestBodyCollector).
  GURL post_url = embedded_test_server()->GetURL("/rum-post");
  NavigateToURLBlockUntilNavigationsComplete(shell(), page_url, 1);
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

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

  auto_continue_paused_ = true;

  // Same-origin POST + JSON body → ResourceRequest has request_body → Collect.
  std::string script = base::StringPrintf(
      R"((async () => {
        const tasks = [];
        for (let i = 0; i < 30; i++) {
          tasks.push(
              fetch('%s-' + i, {
                method: 'POST',
                credentials: 'omit',
                headers: {'content-type': 'application/json'},
                body: JSON.stringify({session: 'asan-repro', n: i}),
              }).catch(() => null));
        }
        await Promise.allSettled(tasks);
        return 'done';
      })())",
      post_url.spec().c_str());

  content::ExecuteScriptAsync(shell()->web_contents(), script);

  base::TimeTicks deadline = base::TimeTicks::Now() + base::Seconds(45);
  while (base::TimeTicks::Now() < deadline && continued_ < 5) {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(50));
    run_loop.Run();
  }

  auto_continue_paused_ = false;

  // Under ASAN + bug: heap-use-after-free abort during re-entrant continue.
  // Under WeakPtr fix: survive with continues observed.
  EXPECT_GE(continued_, 1);
  SendCommandSync("Fetch.disable");
}

}  // namespace content
