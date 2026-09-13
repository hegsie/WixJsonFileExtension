// Self-contained unit tests for the JSON custom-action transform logic.
//
// These compile the production transform sources (see jsonca.tests.vcxproj) and call the real
// file-based functions against temp files, then assert on the resulting JSON. No external test
// framework is used. Results are written as JUnit XML (path from argv[1], default "cpp-tests.xml")
// so CI can publish them as a PR check; the process exit code is the number of failed tests.

#include "JsonFile.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <atomic>
#include <chrono>
#include <iterator>

struct TestResult
{
    std::string name;
    bool failed = false;
    std::string message;
};

static std::vector<TestResult> g_results;
static size_t g_currentIndex = 0;
static int g_pass = 0;
static int g_fail = 0;

static void RecordFailure(const char* file, int line, const std::string& expr)
{
    ++g_fail;
    std::printf("FAIL %s:%d: %s\n", file, line, expr.c_str());
    if (g_currentIndex < g_results.size())
    {
        g_results[g_currentIndex].failed = true;
        g_results[g_currentIndex].message += expr + " (" + file + ":" + std::to_string(line) + ")\n";
    }
}

#define CHECK(cond)                                                                            \
    do {                                                                                       \
        if (cond) { ++g_pass; }                                                                \
        else { RecordFailure(__FILE__, __LINE__, #cond); }                                     \
    } while (0)

#define CHECK_HR(expr)                                                                         \
    do {                                                                                       \
        HRESULT _hr = (expr);                                                                  \
        if (SUCCEEDED(_hr)) { ++g_pass; }                                                      \
        else {                                                                                 \
            char _buf[300];                                                                    \
            std::snprintf(_buf, sizeof(_buf), "%s -> HRESULT 0x%08X", #expr, (unsigned)_hr);   \
            RecordFailure(__FILE__, __LINE__, _buf);                                           \
        }                                                                                      \
    } while (0)

static std::atomic<int> g_counter{ 0 };

static std::wstring WriteTempJson(const std::string& content)
{
    long long ticks = static_cast<long long>(std::chrono::steady_clock::now().time_since_epoch().count());
    std::wstring name = L"jsonca_test_" + std::to_wstring(ticks) +
                        L"_" + std::to_wstring(g_counter.fetch_add(1)) + L".json";
    fs::path p = fs::temp_directory_path() / name;
    std::ofstream os(p, std::ios::binary | std::ios::trunc);
    os << content;
    os.close();
    return p.wstring();
}

static json ReadJson(const std::wstring& path)
{
    std::ifstream is{ fs::path(path) };
    return json::parse(is);
}

static void RemoveFile(const std::wstring& path)
{
    std::error_code ec;
    fs::remove(fs::path(path), ec);
}

static int FlagFor(int bitPosition) { return 1 << bitPosition; }

static void Test_SetValue_UpdatesExisting()
{
    auto path = WriteTempJson(R"({"config":{"value":"old"}})");
    CHECK_HR(UpdateJsonFile(path.c_str(), L"$.config.value", L"new", FlagFor(FLAG_SETVALUE), -1, L""));
    auto j = ReadJson(path);
    CHECK(j["config"]["value"].as<std::string>() == "new");
    RemoveFile(path);
}

static void Test_CreatePointer_CreatesNestedPath()
{
    auto path = WriteTempJson(R"({})");
    CHECK_HR(UpdateJsonFile(path.c_str(), L"/a/b/c", L"v", FlagFor(FLAG_CREATEVALUE), -1, L""));
    auto j = ReadJson(path);
    CHECK(j.contains("a") && j["a"].contains("b") && j["a"]["b"]["c"].as<std::string>() == "v");
    RemoveFile(path);
}

static void Test_DeleteValue_RemovesKey()
{
    auto path = WriteTempJson(R"({"a":1,"b":2})");
    CHECK_HR(UpdateJsonFile(path.c_str(), L"$.a", L"", FlagFor(FLAG_DELETEVALUE), -1, L""));
    auto j = ReadJson(path);
    CHECK(!j.contains("a"));
    CHECK(j.contains("b"));
    RemoveFile(path);
}

static void Test_AppendArray_AddsElement()
{
    auto path = WriteTempJson(R"({"items":[1,2]})");
    CHECK_HR(UpdateJsonFile(path.c_str(), L"$.items", L"3", FlagFor(FLAG_APPENDARRAY), -1, L""));
    auto j = ReadJson(path);
    CHECK(j["items"].size() == 3);
    RemoveFile(path);
}

static void Test_InsertArray_AtIndex()
{
    auto path = WriteTempJson(R"({"items":[1,3]})");
    CHECK_HR(UpdateJsonFile(path.c_str(), L"$.items", L"2", FlagFor(FLAG_INSERTARRAY), 1, L""));
    auto j = ReadJson(path);
    CHECK(j["items"].size() == 3);
    CHECK(j["items"][1].as<int>() == 2);
    RemoveFile(path);
}

static void Test_OnlyIfExists_SkipsMissingPath()
{
    auto path = WriteTempJson(R"({"config":{"value":"old"}})");
    int flags = FlagFor(FLAG_SETVALUE) | FlagFor(FLAG_ONLYIFEXISTS);
    CHECK_HR(UpdateJsonFile(path.c_str(), L"$.config.missing", L"x", flags, -1, L""));
    auto j = ReadJson(path);
    CHECK(!j["config"].contains("missing"));
    RemoveFile(path);
}

static void Test_OnlyIfExists_AppliesWhenPresent()
{
    auto path = WriteTempJson(R"({"config":{"value":"old"}})");
    int flags = FlagFor(FLAG_SETVALUE) | FlagFor(FLAG_ONLYIFEXISTS);
    CHECK_HR(UpdateJsonFile(path.c_str(), L"$.config.value", L"new", flags, -1, L""));
    auto j = ReadJson(path);
    CHECK(j["config"]["value"].as<std::string>() == "new");
    RemoveFile(path);
}

static void Test_SetValue_PreservesStringType()
{
    // Replacing an existing string with something that parses as JSON must stay a string.
    auto path = WriteTempJson(R"({"version":"1.0"})");
    CHECK_HR(UpdateJsonFile(path.c_str(), L"$.version", L"2.5", FlagFor(FLAG_SETVALUE), -1, L""));
    auto j = ReadJson(path);
    CHECK(j["version"].is_string());
    CHECK(j["version"].as<std::string>() == "2.5");
    RemoveFile(path);
}

static void Test_SetValue_WritesTypedValueForNonStrings()
{
    // Replacing a number/boolean takes the parsed (typed) form of the authored value.
    auto path = WriteTempJson(R"({"port":8080,"enabled":false})");
    CHECK_HR(UpdateJsonFile(path.c_str(), L"$.port", L"9090", FlagFor(FLAG_SETVALUE), -1, L""));
    CHECK_HR(UpdateJsonFile(path.c_str(), L"$.enabled", L"true", FlagFor(FLAG_SETVALUE), -1, L""));
    auto j = ReadJson(path);
    CHECK(j["port"].is_number());
    CHECK(j["port"].as<int>() == 9090);
    CHECK(j["enabled"].is_bool());
    CHECK(j["enabled"].as<bool>() == true);
    RemoveFile(path);
}

static void Test_CreatePointer_UpdatesExistingValue()
{
    // createJsonPointerValue is set-or-create: an existing value is replaced, not left as-is.
    auto path = WriteTempJson(R"({"a":{"b":"old"}})");
    CHECK_HR(UpdateJsonFile(path.c_str(), L"/a/b", L"new", FlagFor(FLAG_CREATEVALUE), -1, L""));
    auto j = ReadJson(path);
    CHECK(j["a"]["b"].as<std::string>() == "new");
    RemoveFile(path);
}

static void Test_CreatePointer_TypedValueForNewPath()
{
    // A newly created value has no existing type to preserve, so JSON-parseable text is typed.
    auto path = WriteTempJson(R"({})");
    CHECK_HR(UpdateJsonFile(path.c_str(), L"/timeout", L"30", FlagFor(FLAG_CREATEVALUE), -1, L""));
    auto j = ReadJson(path);
    CHECK(j["timeout"].is_number());
    CHECK(j["timeout"].as<int>() == 30);
    RemoveFile(path);
}

static void Test_OnlyIfExists_SkipsMissingFile()
{
    // A missing file with OnlyIfExists is a successful no-op, not a failed install.
    fs::path missing = fs::temp_directory_path() / L"jsonca_test_missing_file.json";
    RemoveFile(missing.wstring());
    int flags = FlagFor(FLAG_SETVALUE) | FlagFor(FLAG_ONLYIFEXISTS);
    CHECK_HR(UpdateJsonFile(missing.wstring().c_str(), L"$.config.value", L"x", flags, -1, L""));
    CHECK(!fs::exists(missing));
}

static void Test_RemoveArrayElement_ByValue()
{
    auto path = WriteTempJson(R"({"items":["a","b","a"]})");
    CHECK_HR(UpdateJsonFile(path.c_str(), L"$.items", L"a", FlagFor(FLAG_REMOVEARRAYELEMENT), -1, L""));
    auto j = ReadJson(path);
    CHECK(j["items"].size() == 1);
    CHECK(j["items"][0].as<std::string>() == "b");
    RemoveFile(path);
}

static void Test_DistinctArray_RemovesDuplicates()
{
    auto path = WriteTempJson(R"({"items":[1,2,1,3,2]})");
    CHECK_HR(UpdateJsonFile(path.c_str(), L"$.items", L"", FlagFor(FLAG_DISTINCTVALUES), -1, L""));
    auto j = ReadJson(path);
    CHECK(j["items"].size() == 3);
    RemoveFile(path);
}

static void Test_Write_LeavesNoTempFile()
{
    auto path = WriteTempJson(R"({"config":{"value":"old"}})");
    CHECK_HR(UpdateJsonFile(path.c_str(), L"$.config.value", L"new", FlagFor(FLAG_SETVALUE), -1, L""));
    fs::path tempPath(path);
    tempPath += L".wixjson.tmp";
    CHECK(!fs::exists(tempPath));
    RemoveFile(path);
}

static void Test_Schema_ValidPasses_InvalidFails()
{
    auto schemaPath = WriteTempJson(
        R"({"type":"object","required":["name"],"properties":{"name":{"type":"string"}}})");

    auto goodPath = WriteTempJson(R"({"name":"abc"})");
    CHECK_HR(ValidateJsonSchema(goodPath.c_str(), schemaPath.c_str()));

    auto badPath = WriteTempJson(R"({"name":123})");
    CHECK(FAILED(ValidateJsonSchema(badPath.c_str(), schemaPath.c_str())));

    RemoveFile(schemaPath);
    RemoveFile(goodPath);
    RemoveFile(badPath);
}

// Diagnostics: dry run, before/after trace and the transform log (JSONEXT_DRYRUN, JSONEXT_LOGLEVEL,
// JSONEXT_TRANSFORMLOG).
static void Test_DryRun_LeavesFileUntouched()
{
    auto path = WriteTempJson(R"({"config":{"value":"old"}})");
    JSON_OPERATION_TRACE trace;
    CHECK_HR(UpdateJsonFile(path.c_str(), L"$.config.value", L"new", FlagFor(FLAG_SETVALUE), -1, L"", JSON_OPTION_DRYRUN, &trace));
    auto j = ReadJson(path);
    CHECK(j["config"]["value"].as<std::string>() == "old");
    CHECK(trace.outcome == "dry-run");
    CHECK(trace.before == R"(["old"])");
    CHECK(trace.after.empty());
    RemoveFile(path);
}

static void Test_DryRun_ReportsMissingFile()
{
    // A dry run still reports (and fails on) a missing file the way the real run would.
    std::wstring missing = fs::temp_directory_path().wstring() + L"\\jsonca_missing_dryrun.json";
    JSON_OPERATION_TRACE trace;
    HRESULT hr = UpdateJsonFile(missing.c_str(), L"$.a", L"1", FlagFor(FLAG_SETVALUE), -1, L"", JSON_OPTION_DRYRUN, &trace);
    CHECK(FAILED(hr));
    CHECK(trace.outcome == "failed");
    CHECK(trace.before == "<file not found>");
}

static void Test_Trace_CapturesBeforeAndAfter()
{
    auto path = WriteTempJson(R"({"config":{"value":"old","n":1}})");
    JSON_OPERATION_TRACE trace;
    CHECK_HR(UpdateJsonFile(path.c_str(), L"$.config.value", L"new", FlagFor(FLAG_SETVALUE), -1, L"", 0, &trace));
    CHECK(trace.outcome == "applied");
    CHECK(trace.before == R"(["old"])");
    CHECK(trace.after == R"(["new"])");

    // JSON Pointer actions describe the single value rather than a match array.
    JSON_OPERATION_TRACE ptr;
    CHECK_HR(UpdateJsonFile(path.c_str(), L"/config/n", L"2", FlagFor(FLAG_CREATEVALUE), -1, L"", 0, &ptr));
    CHECK(ptr.before == "1");
    CHECK(ptr.after == "2");

    // A skipped OnlyIfExists operation says so and captures no "after".
    JSON_OPERATION_TRACE skipped;
    CHECK_HR(UpdateJsonFile(path.c_str(), L"$.config.missing", L"x", FlagFor(FLAG_SETVALUE) | FlagFor(FLAG_ONLYIFEXISTS), -1, L"", 0, &skipped));
    CHECK(skipped.outcome == "skipped");
    CHECK(skipped.before == "<no match>");
    CHECK(skipped.after.empty());
    RemoveFile(path);
}

static void Test_DescribeJsonAtPath_Markers()
{
    auto path = WriteTempJson(R"({"a":[1,2]})");
    CHECK(DescribeJsonAtPath(path.c_str(), "$.a", false) == "[[1,2]]");
    CHECK(DescribeJsonAtPath(path.c_str(), "/a/1", true) == "2");
    CHECK(DescribeJsonAtPath(path.c_str(), "$.zzz", false) == "<no match>");
    CHECK(DescribeJsonAtPath(path.c_str(), "/zzz", true) == "<no match>");
    CHECK(DescribeJsonAtPath(L"C:\\does\\not\\exist\\x.json", "$.a", false) == "<file not found>");
    RemoveFile(path);

    auto bad = WriteTempJson("{ not json");
    CHECK(DescribeJsonAtPath(bad.c_str(), "$.a", false) == "<not valid JSON>");
    RemoveFile(bad);
}

static void Test_TransformLog_AppendsEntries()
{
    std::wstring logPath = fs::temp_directory_path().wstring() + L"\\jsonca_transform_" +
        std::to_wstring(g_counter.fetch_add(1)) + L".json";
    RemoveFile(logPath);

    json first; first["action"] = "setValue"; first["outcome"] = "applied";
    json second; second["action"] = "deleteValue"; second["outcome"] = "skipped";
    CHECK_HR(AppendTransformLogEntry(logPath.c_str(), first));
    CHECK_HR(AppendTransformLogEntry(logPath.c_str(), second));

    auto log = ReadJson(logPath);
    CHECK(log.is_array());
    CHECK(log.size() == 2);
    CHECK(log[0]["action"].as<std::string>() == "setValue");
    CHECK(log[1]["outcome"].as<std::string>() == "skipped");

    // A log that is not an array is replaced rather than corrupted.
    {
        std::ofstream os(fs::path(logPath), std::ios::binary | std::ios::trunc);
        os << R"({"not":"an array"})";
    }
    CHECK_HR(AppendTransformLogEntry(logPath.c_str(), first));
    log = ReadJson(logPath);
    CHECK(log.is_array() && log.size() == 1);
    RemoveFile(logPath);
}

static void Test_ActionName_FromFlags()
{
    CHECK(std::string(JsonActionName(FlagFor(FLAG_SETVALUE))) == "setValue");
    CHECK(std::string(JsonActionName(FlagFor(FLAG_SETVALUE) | FlagFor(FLAG_ONLYIFEXISTS))) == "setValue");
    CHECK(std::string(JsonActionName(FlagFor(FLAG_CREATEVALUE))) == "createJsonPointerValue");
    CHECK(std::string(JsonActionName(FlagFor(FLAG_DISTINCTVALUES))) == "distinctValues");
    CHECK(std::string(JsonActionName(FlagFor(FLAG_VALIDATESCHEMA))) == "unknown");
}

// CreateBackup / RestoreOnUninstall helpers (JsonBackup.cpp).
static std::string ReadText(const std::wstring& path)
{
    std::ifstream is(fs::path(path), std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
}

static void Test_Backup_CreatesOnceAndKeepsOriginal()
{
    auto path = WriteTempJson(R"({"v":1})");
    std::wstring backup = JsonBackupPath(path.c_str(), L".bak");
    RemoveFile(backup);

    bool created = false;
    CHECK_HR(BackupJsonFile(path.c_str(), L".bak", &created));
    CHECK(created);
    CHECK(fs::exists(fs::path(backup)));
    CHECK(ReadText(backup) == R"({"v":1})");

    // Modify the file, then ask again: the existing backup is kept, not overwritten.
    CHECK_HR(UpdateJsonFile(path.c_str(), L"$.v", L"2", FlagFor(FLAG_SETVALUE), -1, L""));
    created = true;
    HRESULT hr = BackupJsonFile(path.c_str(), L".bak", &created);
    CHECK(S_FALSE == hr);
    CHECK(!created);
    CHECK(ReadText(backup) == R"({"v":1})");

    RemoveFile(backup);
    RemoveFile(path);
}

static void Test_Backup_DefaultSuffixAndMissingFile()
{
    CHECK(JsonBackupPath(L"C:\\x\\a.json", NULL) == L"C:\\x\\a.json.wixbak");
    CHECK(JsonBackupPath(L"C:\\x\\a.json", L"") == L"C:\\x\\a.json.wixbak");
    CHECK(JsonBackupPath(L"C:\\x\\a.json", L".orig") == L"C:\\x\\a.json.orig");

    std::wstring missing = fs::temp_directory_path().wstring() + L"\\jsonca_missing_backup.json";
    bool created = true;
    CHECK(S_FALSE == BackupJsonFile(missing.c_str(), NULL, &created));
    CHECK(!created);
}

static void Test_Restore_PutsBackupBackAndRemovesIt()
{
    auto path = WriteTempJson(R"({"v":1})");
    std::wstring backup = JsonBackupPath(path.c_str(), NULL);
    RemoveFile(backup);
    CHECK_HR(BackupJsonFile(path.c_str(), NULL, NULL));
    CHECK_HR(UpdateJsonFile(path.c_str(), L"$.v", L"2", FlagFor(FLAG_SETVALUE), -1, L""));
    CHECK(ReadJson(path)["v"].as<int>() == 2);

    bool restored = false;
    CHECK_HR(RestoreJsonFileBackup(path.c_str(), NULL, &restored));
    CHECK(restored);
    CHECK(ReadText(path) == R"({"v":1})");
    CHECK(!fs::exists(fs::path(backup)));

    // No backup left: restoring again is a no-op that reports S_FALSE.
    restored = true;
    CHECK(S_FALSE == RestoreJsonFileBackup(path.c_str(), NULL, &restored));
    CHECK(!restored);
    RemoveFile(path);
}

// Timing (On column) gating shared by the scheduling and readValue custom actions. The component
// state pairs mirror what MsiGetComponentState reports: fresh install (absent -> local), repair
// (local -> local), uninstall (local -> absent) and a component that is not part of the transaction
// (local -> unknown, i.e. no change requested).
static void Test_Timing_InstallRow_RunsOnlyInInstallPhase()
{
    CHECK(JsonRowRunsInPhase(TIMING_INSTALL, INSTALLSTATE_ABSENT, INSTALLSTATE_LOCAL, jpInstall));
    CHECK(JsonRowRunsInPhase(TIMING_INSTALL, INSTALLSTATE_LOCAL, INSTALLSTATE_LOCAL, jpInstall));   // repair
    CHECK(!JsonRowRunsInPhase(TIMING_INSTALL, INSTALLSTATE_LOCAL, INSTALLSTATE_ABSENT, jpInstall)); // uninstalling
    CHECK(!JsonRowRunsInPhase(TIMING_INSTALL, INSTALLSTATE_ABSENT, INSTALLSTATE_LOCAL, jpUninstall));
    CHECK(!JsonRowRunsInPhase(TIMING_INSTALL, INSTALLSTATE_LOCAL, INSTALLSTATE_ABSENT, jpUninstall));
}

static void Test_Timing_UninstallRow_RunsOnlyInUninstallPhase()
{
    CHECK(JsonRowRunsInPhase(TIMING_UNINSTALL, INSTALLSTATE_LOCAL, INSTALLSTATE_ABSENT, jpUninstall));
    CHECK(JsonRowRunsInPhase(TIMING_UNINSTALL, INSTALLSTATE_SOURCE, INSTALLSTATE_REMOVED, jpUninstall));
    CHECK(!JsonRowRunsInPhase(TIMING_UNINSTALL, INSTALLSTATE_ABSENT, INSTALLSTATE_LOCAL, jpUninstall)); // installing
    CHECK(!JsonRowRunsInPhase(TIMING_UNINSTALL, INSTALLSTATE_LOCAL, INSTALLSTATE_LOCAL, jpUninstall));  // repair
    CHECK(!JsonRowRunsInPhase(TIMING_UNINSTALL, INSTALLSTATE_ABSENT, INSTALLSTATE_LOCAL, jpInstall));
    CHECK(!JsonRowRunsInPhase(TIMING_UNINSTALL, INSTALLSTATE_LOCAL, INSTALLSTATE_ABSENT, jpInstall));
}

static void Test_Timing_BothRow_RunsInEachMatchingPhase()
{
    const int both = TIMING_INSTALL | TIMING_UNINSTALL;
    CHECK(JsonRowRunsInPhase(both, INSTALLSTATE_ABSENT, INSTALLSTATE_LOCAL, jpInstall));
    CHECK(JsonRowRunsInPhase(both, INSTALLSTATE_LOCAL, INSTALLSTATE_LOCAL, jpInstall));
    CHECK(JsonRowRunsInPhase(both, INSTALLSTATE_LOCAL, INSTALLSTATE_ABSENT, jpUninstall));
    // The phase still has to match the component transition: an install-phase scheduler never
    // runs a row of a component being removed, and vice versa.
    CHECK(!JsonRowRunsInPhase(both, INSTALLSTATE_LOCAL, INSTALLSTATE_ABSENT, jpInstall));
    CHECK(!JsonRowRunsInPhase(both, INSTALLSTATE_ABSENT, INSTALLSTATE_LOCAL, jpUninstall));
}

static void Test_Timing_NullColumn_MeansInstall()
{
    CHECK(JsonRowRunsInPhase(MSI_NULL_INTEGER, INSTALLSTATE_ABSENT, INSTALLSTATE_LOCAL, jpInstall));
    CHECK(!JsonRowRunsInPhase(MSI_NULL_INTEGER, INSTALLSTATE_LOCAL, INSTALLSTATE_ABSENT, jpUninstall));
}

static void Test_Timing_UnchangedComponent_NeverRuns()
{
    // INSTALLSTATE_UNKNOWN action means the component is not touched by this transaction.
    const int both = TIMING_INSTALL | TIMING_UNINSTALL;
    CHECK(!JsonRowRunsInPhase(both, INSTALLSTATE_LOCAL, INSTALLSTATE_UNKNOWN, jpInstall));
    CHECK(!JsonRowRunsInPhase(both, INSTALLSTATE_LOCAL, INSTALLSTATE_UNKNOWN, jpUninstall));
    CHECK(!JsonRowRunsInPhase(both, INSTALLSTATE_ABSENT, INSTALLSTATE_UNKNOWN, jpInstall));
    CHECK(!JsonRowRunsInPhase(both, INSTALLSTATE_ABSENT, INSTALLSTATE_ABSENT, jpUninstall)); // never installed
}

static void RunTest(const char* name, void (*fn)())
{
    g_results.push_back(TestResult{ name });
    g_currentIndex = g_results.size() - 1;
    fn();
}

static std::string XmlEscape(const std::string& s)
{
    std::string out;
    for (char c : s)
    {
        switch (c)
        {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out += c; break;
        }
    }
    return out;
}

static void WriteJUnit(const std::string& path)
{
    int failures = 0;
    for (const auto& r : g_results)
    {
        if (r.failed) ++failures;
    }

    std::ofstream os(path, std::ios::binary | std::ios::trunc);
    if (!os.is_open())
    {
        std::printf("WARN: could not write JUnit results to %s\n", path.c_str());
        return;
    }

    os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    os << "<testsuites>\n";
    os << "  <testsuite name=\"jsonca\" tests=\"" << g_results.size() << "\" failures=\"" << failures << "\">\n";
    for (const auto& r : g_results)
    {
        os << "    <testcase classname=\"jsonca\" name=\"" << XmlEscape(r.name) << "\">";
        if (r.failed)
        {
            os << "\n      <failure message=\"" << XmlEscape(r.message) << "\"></failure>\n    ";
        }
        os << "</testcase>\n";
    }
    os << "  </testsuite>\n";
    os << "</testsuites>\n";
    os.close();
}

int main(int argc, char** argv)
{
    RunTest("SetValue_UpdatesExisting", Test_SetValue_UpdatesExisting);
    RunTest("CreatePointer_CreatesNestedPath", Test_CreatePointer_CreatesNestedPath);
    RunTest("DeleteValue_RemovesKey", Test_DeleteValue_RemovesKey);
    RunTest("AppendArray_AddsElement", Test_AppendArray_AddsElement);
    RunTest("InsertArray_AtIndex", Test_InsertArray_AtIndex);
    RunTest("OnlyIfExists_SkipsMissingPath", Test_OnlyIfExists_SkipsMissingPath);
    RunTest("OnlyIfExists_AppliesWhenPresent", Test_OnlyIfExists_AppliesWhenPresent);
    RunTest("SetValue_PreservesStringType", Test_SetValue_PreservesStringType);
    RunTest("SetValue_WritesTypedValueForNonStrings", Test_SetValue_WritesTypedValueForNonStrings);
    RunTest("CreatePointer_UpdatesExistingValue", Test_CreatePointer_UpdatesExistingValue);
    RunTest("CreatePointer_TypedValueForNewPath", Test_CreatePointer_TypedValueForNewPath);
    RunTest("OnlyIfExists_SkipsMissingFile", Test_OnlyIfExists_SkipsMissingFile);
    RunTest("RemoveArrayElement_ByValue", Test_RemoveArrayElement_ByValue);
    RunTest("DistinctArray_RemovesDuplicates", Test_DistinctArray_RemovesDuplicates);
    RunTest("Write_LeavesNoTempFile", Test_Write_LeavesNoTempFile);
    RunTest("Schema_ValidPasses_InvalidFails", Test_Schema_ValidPasses_InvalidFails);
    RunTest("Timing_InstallRow_RunsOnlyInInstallPhase", Test_Timing_InstallRow_RunsOnlyInInstallPhase);
    RunTest("Timing_UninstallRow_RunsOnlyInUninstallPhase", Test_Timing_UninstallRow_RunsOnlyInUninstallPhase);
    RunTest("Timing_BothRow_RunsInEachMatchingPhase", Test_Timing_BothRow_RunsInEachMatchingPhase);
    RunTest("Timing_NullColumn_MeansInstall", Test_Timing_NullColumn_MeansInstall);
    RunTest("Timing_UnchangedComponent_NeverRuns", Test_Timing_UnchangedComponent_NeverRuns);
    RunTest("DryRun_LeavesFileUntouched", Test_DryRun_LeavesFileUntouched);
    RunTest("DryRun_ReportsMissingFile", Test_DryRun_ReportsMissingFile);
    RunTest("Trace_CapturesBeforeAndAfter", Test_Trace_CapturesBeforeAndAfter);
    RunTest("DescribeJsonAtPath_Markers", Test_DescribeJsonAtPath_Markers);
    RunTest("TransformLog_AppendsEntries", Test_TransformLog_AppendsEntries);
    RunTest("ActionName_FromFlags", Test_ActionName_FromFlags);
    RunTest("Backup_CreatesOnceAndKeepsOriginal", Test_Backup_CreatesOnceAndKeepsOriginal);
    RunTest("Backup_DefaultSuffixAndMissingFile", Test_Backup_DefaultSuffixAndMissingFile);
    RunTest("Restore_PutsBackupBackAndRemovesIt", Test_Restore_PutsBackupBackAndRemovesIt);

    std::string out = (argc > 1) ? argv[1] : "cpp-tests.xml";
    WriteJUnit(out);

    std::printf("\njsonca unit tests: %d passed, %d failed (results: %s)\n", g_pass, g_fail, out.c_str());
    return g_fail == 0 ? 0 : 1;
}
