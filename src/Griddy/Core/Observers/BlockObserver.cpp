#include "BlockObserver.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../Grid.hpp"
#include "../Objects/Terrain/Minerals.hpp"
#include "Vulkan/VulkanDevice.hpp"

namespace griddy {

BlockObserver::BlockObserver(std::shared_ptr<Grid> grid, uint32_t tileSize) : VulkanObserver(grid, tileSize) {
}

BlockObserver::~BlockObserver() {
}

void BlockObserver::init(uint gridWidth, uint gridHeight) {
  VulkanObserver::init(gridWidth, gridHeight);

  device_->initRenderMode(vk::RenderMode::SHAPES);

}

std::unique_ptr<uint8_t[]> BlockObserver::reset() const {
  auto ctx = device_->beginRender();

  render(ctx);

  auto width = grid_->getWidth() * tileSize_;
  auto height = grid_->getHeight() * tileSize_;

  // Only update the rectangles that have changed to save bandwidth/processing speed
  std::vector<VkRect2D> dirtyRectangles = {
      {{0, 0},
       {width, height}}};

  return device_->endRender(ctx, dirtyRectangles);
}

std::unique_ptr<uint8_t[]> BlockObserver::update(int playerId) const {
  auto ctx = device_->beginRender();

  render(ctx);
  // Only update the rectangles that have changed to save bandwidth/processing speed
  std::vector<VkRect2D> dirtyRectangles;

  auto updatedLocations = grid_->getUpdatedLocations();

  for (auto l : updatedLocations) {
    VkOffset2D offset = {l.x * tileSize_, l.y * tileSize_};
    VkExtent2D extent = {tileSize_, tileSize_};

    dirtyRectangles.push_back({offset, extent});
  }

  return device_->endRender(ctx, dirtyRectangles);
}

void BlockObserver::render(vk::VulkanRenderContext& ctx) const {
  auto width = grid_->getWidth();
  auto height = grid_->getHeight();

  auto offset = (float)tileSize_ / 2.0f;

  auto square = device_->getShapeBuffer("square");
  auto triangle = device_->getShapeBuffer("triangle");

  auto objects = grid_->getObjects();

  for (const auto& object : objects) {
    float scale = (float)tileSize_;
    auto location = object->getLocation();
    auto objectType = object->getObjectType();

    vk::ShapeBuffer* shapeBuffer;
    glm::vec3 color = {};
    switch (objectType) {
      case HARVESTER:
        color = {0.6, 0.2, 0.2};
        shapeBuffer = &square;
        scale *= 0.7;
        break;
      case MINERALS: {
        color = {0.0, 1.0, 0.0};
        shapeBuffer = &triangle;
        auto minerals = std::dynamic_pointer_cast<Minerals>(object);
        scale *= ((float)minerals->getValue() / minerals->getMaxValue());
      } break;
      case PUSHER:
        color = {0.2, 0.2, 0.6};
        shapeBuffer = &square;
        scale *= 0.8;
        break;
      case PUNCHER:
        color = {0.2, 0.6, 0.6};
        shapeBuffer = &square;
        scale *= 0.8;
        break;
      case FIXED_WALL:
        color = {0.5, 0.5, 0.5};
        shapeBuffer = &square;
        break;
      case PUSHABLE_WALL:
        color = {0.8, 0.8, 0.8};
        shapeBuffer = &square;
        break;
    }

    glm::vec3 position = {offset + location.x * tileSize_, offset + location.y * tileSize_, -1.0f};
    glm::mat4 model = glm::scale(glm::translate(glm::mat4(1.0f), position), glm::vec3(scale));
    device_->drawShape(ctx, *shapeBuffer, model, color);
  }
}

}  // namespace griddy