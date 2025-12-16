#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "../Models/Project_path.h"

// Отвечает за обработку настроек из "settings.json"
class Config
{
  public:
    Config()
    {
        reload();
    }
    // Загружает настройки из "settings.json"
    void reload()
    {
        std::ifstream fin(project_path + "settings.json");
        fin >> config; // сохраняет в config
        fin.close();
    }

    // Перегруженный оператор круглые скобки () обеспечивает удобный доступ
    // к значениям настроек по иерархии JSON
    auto operator()(const string &setting_dir, const string &setting_name) const
    {
        return config[setting_dir][setting_name];
    }

  private:
    json config;
};