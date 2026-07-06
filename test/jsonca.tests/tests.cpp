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

    std::string out = (argc > 1) ? argv[1] : "cpp-tests.xml";
    WriteJUnit(out);

    std::printf("\njsonca unit tests: %d passed, %d failed (results: %s)\n", g_pass, g_fail, out.c_str());
    return g_fail == 0 ? 0 : 1;
}
