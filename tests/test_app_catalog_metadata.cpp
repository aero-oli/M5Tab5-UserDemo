/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include <cassert>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct CatalogEntry {
    std::string key;
    std::string category;
    std::string label;
    std::string iconExpression;
    int order             = 0;
    int summaryPriority   = 0;
    bool dangerous        = false;
    bool dashboardVisible = false;
};

std::string readFile(const fs::path& path)
{
    std::ifstream input(path);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::vector<std::string> appKeysFromRegistry(const std::string& registryHeader)
{
    std::smatch match;
    const std::regex enumRegex(R"(enum\s+class\s+AppKey\s*\{([^}]*)\})", std::regex::multiline);
    assert(std::regex_search(registryHeader, match, enumRegex));

    std::vector<std::string> keys;
    const std::string body = match[1].str();
    const std::regex keyRegex(R"(\b([A-Z][A-Za-z0-9_]*)\b)");
    for (auto it = std::sregex_iterator(body.begin(), body.end(), keyRegex); it != std::sregex_iterator(); ++it) {
        keys.push_back((*it)[1].str());
    }
    return keys;
}

std::vector<CatalogEntry> catalogEntries(const std::string& catalogSource)
{
    std::vector<CatalogEntry> entries;
    const std::regex entryRegex(
        R"REGEX(apps\.add\(\{AppKey::([A-Za-z0-9_]+),\s*AppCategory::([A-Za-z0-9_]+),\s*"([^"]+)",\s*"[^"]*",\s*(?:"[^"]*"|[^,]+),\s*("([^"]+)"|LV_SYMBOL_[A-Z0-9_]+),\s*(-?\d+),\s*(-?\d+),\s*(true|false),\s*(true|false),)REGEX",
        std::regex::multiline);

    for (auto it = std::sregex_iterator(catalogSource.begin(), catalogSource.end(), entryRegex);
         it != std::sregex_iterator(); ++it) {
        CatalogEntry entry;
        entry.key              = (*it)[1].str();
        entry.category         = (*it)[2].str();
        entry.label            = (*it)[3].str();
        entry.iconExpression   = (*it)[4].str();
        entry.order            = std::stoi((*it)[6].str());
        entry.summaryPriority  = std::stoi((*it)[7].str());
        entry.dangerous        = (*it)[8].str() == "true";
        entry.dashboardVisible = (*it)[9].str() == "true";
        entries.push_back(entry);
    }
    return entries;
}

const CatalogEntry& requireEntry(const std::map<std::string, CatalogEntry>& entries, const std::string& key)
{
    const auto it = entries.find(key);
    assert(it != entries.end());
    return it->second;
}

void assertCategory(const std::map<std::string, CatalogEntry>& entries, const std::string& key,
                    const std::string& category)
{
    assert(requireEntry(entries, key).category == category);
}

}  // namespace

int main()
{
    const fs::path repoRoot = fs::path(__FILE__).parent_path().parent_path();
    const auto registryKeys = appKeysFromRegistry(readFile(repoRoot / "app" / "apps" / "app_registry.h"));
    const auto entries      = catalogEntries(readFile(repoRoot / "app" / "apps" / "app_catalog.cpp"));

    std::map<std::string, CatalogEntry> byKey;
    for (const auto& entry : entries) {
        assert(!entry.key.empty());
        assert(!entry.category.empty());
        assert(!entry.label.empty());
        assert(!entry.iconExpression.empty());
        assert(entry.iconExpression.rfind("LV_SYMBOL_", 0) == 0);
        assert(entry.order >= 0);
        assert(byKey.emplace(entry.key, entry).second);
    }

    assert(entries.size() == registryKeys.size());
    for (const auto& key : registryKeys) {
        assert(byKey.count(key) == 1);
    }

    for (const auto& key : {"Network", "Updates", "DisplaySound", "SettingsPower", "About", "Diagnostics"}) {
        assertCategory(byKey, key, "Settings");
    }
    for (const auto& key : {"Rtc", "PowerMonitor", "Imu", "Switches", "I2cScan", "Gpio", "SdCard", "ComMonitor"}) {
        assertCategory(byKey, key, "Hardware");
    }
    for (const auto& key : {"Camera", "DualMic", "Headphone", "Music"}) {
        assertCategory(byKey, key, "Media");
    }
    assertCategory(byKey, "Pps", "Modules");

    assert(requireEntry(byKey, "Network").dashboardVisible);
    assert(requireEntry(byKey, "PowerMonitor").summaryPriority > requireEntry(byKey, "Rtc").summaryPriority);
    assert(requireEntry(byKey, "Pps").dashboardVisible);
    assert(requireEntry(byKey, "Updates").dangerous);
    assert(requireEntry(byKey, "SettingsPower").dangerous);
    assert(requireEntry(byKey, "Gpio").dangerous);

    return 0;
}
