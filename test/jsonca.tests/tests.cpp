// Self-contained unit tests for the JSON custom-action transform logic.
//
// These compile the production transform sources (see jsonca.tests.vcxproj) and call the real
// file-based functions against temp files, then assert on the resulting JSON. No external test
// framework is used so the project builds with just the WiX native NuGet packages; the process exit
// code is the number of failures (0 == success), which CI checks.

#include "JsonFile.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <atomic>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond)                                                                            \
    do {                                                                                       \
        if (cond) { ++g_pass; }                                                                \
        else { ++g_fail; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); }         \
    } while (0)

#define CHECK_HR(expr)                                                                         \
    do {                                                                                       \
        HRESULT _hr = (expr);                                                                  \
        if (SUCCEEDED(_hr)) { ++g_pass; }                                                      \
        else { ++g_fail; std::printf("FAIL %s:%d: HRESULT 0x%08X from %s\n",                   \
                                     __FILE__, __LINE__, (unsigned)_hr, #expr); }              \
    } while (0)

static std::atomic<int> g_counter{ 0 };

static std::wstring WriteTempJson(const std::string& content)
{
    std::wstring name = L"jsonca_test_" + std::to_wstring(::GetTickCount64()) +
                        L"_" + std::to_wstring(g_counter.fetch_add(1)) + L".json";
    fs::path p = fs::temp_directory_path() / name;
    std::ofstream os(p, std::ios::binary | std::ios::trunc);
    os << content;
    os.close();
    return p.wstring();
}

static json ReadJson(const std::wstring& path)
{
    std::ifstream is(fs::path(path));
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
    // Path does not exist: with OnlyIfExists the operation must be skipped (and succeed).
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

int main()
{
    Test_SetValue_UpdatesExisting();
    Test_CreatePointer_CreatesNestedPath();
    Test_DeleteValue_RemovesKey();
    Test_AppendArray_AddsElement();
    Test_InsertArray_AtIndex();
    Test_OnlyIfExists_SkipsMissingPath();
    Test_OnlyIfExists_AppliesWhenPresent();
    Test_Schema_ValidPasses_InvalidFails();

    std::printf("\njsonca unit tests: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
