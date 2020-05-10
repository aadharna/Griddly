#include "Griddy/Core/GDY/Objects/ObjectGenerator.hpp"
#include "gmock/gmock.h"

namespace griddy {

class MockObjectGenerator : public ObjectGenerator {
 public:
  MockObjectGenerator() : ObjectGenerator() {}

  MOCK_METHOD(void, defineNewObject, (std::string objectName, uint32_t zIdx, char mapChar, (std::unordered_map<std::string, uint32_t> parameterDefinitions)), ());
  MOCK_METHOD(void, defineActionBehaviour, (std::string objectName, ActionBehaviourDefinition behaviourDefinition), ());

  MOCK_METHOD(std::shared_ptr<Object>, newInstance, (std::string objectName), ());

  MOCK_METHOD(std::string&, getObjectNameFromMapChar, (char character), ());
};
}  // namespace griddy