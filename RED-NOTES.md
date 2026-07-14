# RED notes

Before the fix, the same scenario used EXPECT_CHECK_DEATH_WITH(..., "done_headers_create_new_entry_").

Filter: HttpCacheSimpleGetTest.QueuedDoomRaceAfterDoneHeadersCreateNewEntryRestartsCheckDeath

Mechanism: writers + LOAD_VALIDATE_CACHE paused at connect; deferred BYPASS CreateEntry holds PendingOp; validation no-match sets done_headers_create_new_entry_ and queues doom behind CREATE; OnIOComplete notifies doom with ERR_CACHE_RACE; restart hits DoGetBackendComplete CHECK.

See red-httpcache-done-headers.log (death test PASSED = CHECK fired).
