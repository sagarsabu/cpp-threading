extern "C"
{
/// https://github.com/google/sanitizers/wiki/ThreadSanitizerFlags
/// ignore async safety for now
const char* __tsan_default_options(void) { return "report_signal_unsafe=0"; }
}
