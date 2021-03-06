#pragma once

#include <memory>
#include <vector>

#include "GDY/GDYFactory.hpp"
#include "GDY/TerminationHandler.hpp"
#include "Grid.hpp"
#include "Observers/Observer.hpp"

namespace griddly {

class Player;

struct ActionResult {
  std::unordered_map<uint32_t, TerminationState> playerStates;
  bool terminated;
};

struct ObjectInfo {
  std::string name;
  std::unordered_map<std::string, int32_t> variables;
  glm::ivec2 location;
  DiscreteOrientation orientation;
  uint8_t playerId;
};

struct StateInfo {
  int gameTicks;
  std::unordered_map<std::string, std::unordered_map<uint32_t, int32_t>> globalVariables;
  std::vector<ObjectInfo> objectInfo;
};

class GameProcess : public std::enable_shared_from_this<GameProcess> {
 public:
  GameProcess(ObserverType globalObserverType, std::shared_ptr<GDYFactory> gdyFactory, std::shared_ptr<Grid> grid);

  virtual uint8_t* observe() const;

  virtual ActionResult performActions(uint32_t playerId, std::vector<std::shared_ptr<Action>> actions, bool updateTicks=true) = 0;

  virtual void addPlayer(std::shared_ptr<Player> player);

  // Set the level by its id in the GDY description
  virtual void setLevel(uint32_t levelId);

  // Use a custom level string
  virtual void setLevel(std::string levelString);

  virtual void init(bool isCloned=false);

  virtual void reset();

  bool isInitialized();

  virtual int32_t getAccumulatedRewards(uint32_t playerId);

  virtual std::string getProcessName() const;

  std::shared_ptr<Grid> getGrid();
  std::shared_ptr<Observer> getObserver();

  virtual std::unordered_map<glm::ivec2, std::unordered_set<std::string>> getAvailableActionNames(uint32_t playerId) const;

  virtual std::vector<uint32_t> getAvailableActionIdsAtLocation(glm::ivec2 location, std::string actionName) const;

  virtual StateInfo getState() const;

  virtual uint32_t getNumPlayers() const;

  void release();

  virtual ~GameProcess() = 0;

 protected:


  virtual void setLevelGenerator(std::shared_ptr<LevelGenerator> levelGenerator);
  virtual std::shared_ptr<LevelGenerator> getLevelGenerator() const;

  std::vector<std::shared_ptr<Player>> players_;
  std::shared_ptr<Grid> grid_;
  std::shared_ptr<GDYFactory> gdyFactory_;
  std::shared_ptr<TerminationHandler> terminationHandler_;

  // Game process can have its own observer so we can monitor the game regardless of the player
  ObserverType globalObserverType_;
  std::shared_ptr<Observer> observer_;

  // A level generator used to reset the environment
  std::shared_ptr<LevelGenerator> levelGenerator_;

  bool isInitialized_ = false;

  // Should the game process reset itself or rely on external reset
  bool autoReset_ = false;

  // track whether this environment has finished or not, if it requires a reset, we can reset it
  bool requiresReset_ = true;

  // Tracks the rewards currently accumulated per player
  std::unordered_map<uint32_t, int32_t> accumulatedRewards_;

 private:
  void resetObservers();
  ObserverConfig getObserverConfig(ObserverType observerType) const;
  

};
}  // namespace griddly