#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

#include <fstream>
#include <sstream>

#define SPDLOG_HEADER_ONLY
#include <spdlog/fmt/fmt.h>

#include "../Grid.hpp"
#include "../LevelGenerators/MapReader.hpp"
#include "../Observers/TileObserver.hpp"
#include "../TurnBasedGameProcess.hpp"
#include "GDYFactory.hpp"
#include "Objects/Object.hpp"

namespace griddy {

GDYFactory::GDYFactory() {
#ifndef NDEBUG
  spdlog::set_level(spdlog::level::debug);
#else
  spdlog::set_level(spdlog::level::info);
#endif

  objectGenerator_ = std::shared_ptr<ObjectGenerator>(new ObjectGenerator());
}

GDYFactory::~GDYFactory() {
}

void GDYFactory::createLevel(uint32_t width, uint32_t height, std::shared_ptr<Grid>& grid) {
  grid->init(width, height);
}

void GDYFactory::loadLevel(uint32_t level) {

  if(mapReaderLevelGenerator_ == nullptr) {
    mapReaderLevelGenerator_ = std::shared_ptr<MapReader>(new MapReader(objectGenerator_));
  }

  auto levelString = std::stringstream(levelStrings_[level]);
  mapReaderLevelGenerator_->parseFromStream(levelString);
}

void GDYFactory::initializeFromFile(std::string filename) {
  spdlog::info("Loading gdy file: {0}", filename);
  std::ifstream gdyFile;
  gdyFile.open(filename);
  parseFromStream(gdyFile);
}

void GDYFactory::parseFromStream(std::istream& stream) {
  auto gdyConfig = YAML::Load(stream);

  auto version = gdyConfig["Version"].as<std::string>();

  spdlog::info("Loading GDY file Version: {0}.", version);
  auto environment = gdyConfig["Environment"];
  auto objects = gdyConfig["Objects"];
  auto actions = gdyConfig["Actions"];

  loadObjects(objects);
  loadActions(actions);

  loadEnvironment(environment);
}

void GDYFactory::loadEnvironment(YAML::Node environment) {
  spdlog::info("Loading Environment...");

  tileSize_ = environment["TileSize"].IsDefined() ? environment["TileSize"].as<uint32_t>() : 10;

  auto levels = environment["Levels"];
  for (std::size_t l = 0; l < levels.size(); l++) {
    auto levelString = levels[l].as<std::string>();
    levelStrings_.push_back(levelString);
  }

  spdlog::info("Loaded {0} levels", levelStrings_.size());
}

void GDYFactory::loadObjects(YAML::Node objects) {
  spdlog::info("Loading {0} objects...", objects.size());

  for (std::size_t i = 0; i < objects.size(); i++) {
    auto object = objects[i];
    auto objectName = object["Name"].as<std::string>();
    auto mapChar = object["MapCharacter"].as<char>();
    auto spriteFilename = object["Sprite"].as<std::string>();
    auto blockDefinition = parseBlockObserverDefinition(object["Block"]);
    auto params = object["Parameters"];

    std::unordered_map<std::string, uint32_t> parameterDefinitions;

    for (std::size_t p = 0; p < params.size(); p++) {
      auto param = params[p];
      auto paramName = param["Name"].as<std::string>();
      auto paramInitialValue = param["InitialValue"].as<uint32_t>();

      parameterDefinitions.insert({paramName, paramInitialValue});
    }

    objectGenerator_->defineNewObject(objectName, mapChar, parameterDefinitions);

    blockObserverDefinitions_.insert({objectName, blockDefinition});
    spriteObserverDefinitions_.insert({objectName, spriteFilename});
  }
}

BlockDefinition GDYFactory::parseBlockObserverDefinition(YAML::Node blockNode) {
  BlockDefinition blockDefinition;
  auto colorNode = blockNode["Color"];
  for (std::size_t c = 0; c < colorNode.size(); c++) {
    blockDefinition.color[c] = colorNode[c].as<float>();
  }
  blockDefinition.shape = blockNode["Shape"].as<std::string>();
  blockDefinition.scale = blockNode["Scale"].IsDefined() ? blockNode["Scale"].as<float>() : 1.0;

  return blockDefinition;
}

ActionBehaviourDefinition GDYFactory::makeBehaviourDefinition(ActionBehaviourType behaviourType,
                                                              std::string objectName,
                                                              std::string associatedObjectName,
                                                              std::string actionName,
                                                              std::string commandName,
                                                              std::vector<std::string> commandParameters,
                                                              std::unordered_map<std::string, std::vector<std::string>> conditionalCommands) {
  ActionBehaviourDefinition behaviourDefinition;
  behaviourDefinition.actionName = actionName;
  behaviourDefinition.behaviourType = behaviourType;
  behaviourDefinition.commandName = commandName;
  behaviourDefinition.commandParameters = commandParameters;
  behaviourDefinition.conditionalCommands = conditionalCommands;

  switch (behaviourType) {
    case SOURCE:
      behaviourDefinition.sourceObjectName = objectName;
      behaviourDefinition.destinationObjectName = associatedObjectName;
      break;
    case DESTINATION:
      behaviourDefinition.destinationObjectName = objectName;
      behaviourDefinition.sourceObjectName = associatedObjectName;
      break;
  }

  return behaviourDefinition;
}

void GDYFactory::parseActionBehaviours(ActionBehaviourType actionBehaviourType, std::string objectName, std::string actionName, std::vector<std::string> associatedObjectNames, YAML::Node commands) {
  spdlog::debug("Parsing {0} commands for action {1}, object {2}", commands.size(), actionName, objectName);
  for (std::size_t c = 0; c < commands.size(); c++) {
    auto commandIt = commands[0].begin();
    // iterate through keys
    auto commandName = commandIt->first.as<std::string>();
    auto commandParams = commandIt->second;

    if (commandParams.IsMap()) {
      auto conditionParams = commandParams["Params"];
      auto conditionSubCommands = commandParams["Cmd"];

      auto commandParamStrings = singleOrListNodeToList(conditionParams);

      std::unordered_map<std::string, std::vector<std::string>> parsedSubCommands;
      for (std::size_t sc = 0; sc < conditionSubCommands.size(); sc++) {
        auto subCommandIt = conditionSubCommands[0].begin();
        auto subCommandName = subCommandIt->first.as<std::string>();
        auto subCommandParams = subCommandIt->second;
      }

      for (auto associatedObjectName : associatedObjectNames) {
        auto behaviourDefinition = makeBehaviourDefinition(actionBehaviourType, objectName, associatedObjectName, actionName, commandName, commandParamStrings, parsedSubCommands);

        objectGenerator_->defineActionBehaviour(objectName, behaviourDefinition);
      }

    } else if (commandParams.IsSequence() || commandParams.IsScalar()) {
      auto commandParamStrings = singleOrListNodeToList(commandParams);
      for (auto associatedObjectName : associatedObjectNames) {
        auto behaviourDefinition = makeBehaviourDefinition(actionBehaviourType, objectName, associatedObjectName, actionName, commandName, commandParamStrings, {});
        objectGenerator_->defineActionBehaviour(objectName, behaviourDefinition);
      }
    } else {
      throw std::invalid_argument(fmt::format("Badly defined command {0}", commandName));
    }
  }
}

void GDYFactory::loadActions(YAML::Node actions) {
  spdlog::info("Loading {0} actions...", actions.size());
  for (std::size_t i = 0; i < actions.size(); i++) {
    auto action = actions[i];
    auto actionName = action["Name"].as<std::string>();
    auto behaviours = action["Behaviours"];

    for (std::size_t b = 0; b < behaviours.size(); b++) {
      auto behaviour = behaviours[b];
      auto src = behaviour["Src"];
      auto dst = behaviour["Dst"];

      auto srcTypeNames = singleOrListNodeToList(src["Type"]);
      auto dstTypeNames = singleOrListNodeToList(dst["Type"]);

      for (auto srcName : srcTypeNames) {
        parseActionBehaviours(SOURCE, srcName, actionName, dstTypeNames, src["Cmd"]);
      }

      for (auto dstName : dstTypeNames) {
        parseActionBehaviours(DESTINATION, dstName, actionName, srcTypeNames, dst["Cmd"]);
      }
    }
  }
}

std::vector<std::string> GDYFactory::singleOrListNodeToList(YAML::Node singleOrList) {
  std::vector<std::string> values;
  if (singleOrList.IsScalar()) {
    values.push_back(singleOrList.as<std::string>());
  } else if (singleOrList.IsSequence()) {
    for (std::size_t s = 0; s < singleOrList.size(); s++) {
      values.push_back(singleOrList[s].as<std::string>());
    }
  }

  return values;
}

std::shared_ptr<LevelGenerator> GDYFactory::getLevelGenerator() const {
  return mapReaderLevelGenerator_;
}

std::shared_ptr<ObjectGenerator> GDYFactory::getObjectGenerator() const {
  return objectGenerator_;
}

std::unordered_map<std::string, std::string> GDYFactory::getSpriteObserverDefinitions() const {
  return spriteObserverDefinitions_;
}

std::unordered_map<std::string, BlockDefinition> GDYFactory::getBlockObserverDefinitions() const {
  return blockObserverDefinitions_;
}

uint32_t GDYFactory::getTileSize() const {
  return tileSize_;
}

}  // namespace griddy