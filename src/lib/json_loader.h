#pragma once

#include <filesystem>

#include "model.h"

namespace json_loader
{

std::unique_ptr<model::Game> LoadGame(const std::filesystem::path & json_path);

}  // namespace json_loader
