#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

struct ModelImageEntry
{
    std::string modelName;
    std::string imageUrl;
};

class ModelImageDB
{
public:
    bool load(const std::string& jsonPath)
    {
        std::ifstream f(jsonPath);
        if (!f) return false;
        std::stringstream ss;
        ss << f.rdbuf();
        return parse(ss.str());
    }

    const std::string* lookup(const std::string& modelName) const
    {
        for (auto& e : mEntries)
            if (e.modelName == modelName)
                return &e.imageUrl;
        return nullptr;
    }

    const std::string* lookupPrefix(const std::string& prefix) const
    {
        for (auto& e : mEntries)
            if (e.modelName.find(prefix) == 0)
                return &e.imageUrl;
        return nullptr;
    }

private:
    std::vector<ModelImageEntry> mEntries;

    bool parse(const std::string& json)
    {
        // Minimal parser: scan for "model_name":"...","model_color_id":"...","sca_image_image_url":"..."
        size_t pos = 0;
        while (true)
        {
            auto nameKey = json.find("\"model_name\":\"", pos);
            if (nameKey == std::string::npos) break;
            nameKey += 15;
            auto nameEnd = json.find('"', nameKey);
            if (nameEnd == std::string::npos) break;

            std::string name = json.substr(nameKey, nameEnd - nameKey);

            auto urlKey = json.find("\"sca_image_image_url\":\"", nameEnd);
            if (urlKey == std::string::npos) break;
            urlKey += 22;
            auto urlEnd = json.find('"', urlKey);
            if (urlEnd == std::string::npos) break;

            std::string url = json.substr(urlKey, urlEnd - urlKey);
            mEntries.push_back({name, url});
            pos = urlEnd + 1;
        }
        return !mEntries.empty();
    }
};
