#pragma once

#include "transform.h"

#include <memory>
#include <string>

class SceneNode
{
      public:
        SceneNode() = delete;
        SceneNode(std::uint32_t id, const std::string& name, std::shared_ptr<SceneNode> parent = nullptr)
            : id(id), name(name), parent(parent), meshIndices(), dirtyTransform(true), children(),
              localTransform(Transform<NodeSpace, NodeSpace>::identity())
        {
        }

        SceneNode(SceneNode&) = delete;
        SceneNode& operator=(SceneNode&) = delete;

        SceneNode(SceneNode&&) = delete;
        SceneNode& operator=(SceneNode&&) = delete;

        std::uint32_t getID() const
        {
                return id;
        }

        const std::string& getName() const
        {
                return name;
        }

        const std::vector<std::shared_ptr<SceneNode>> getChildren() const
        {
                return children;
        }

        const std::vector<std::size_t> getMeshIndices() const
        {
                return meshIndices;
        }

        void setLocalPosition(const glm::vec3& pos)
        {
                localPosition = pos;
                dirtyTransform = true;
        }

        void setLocalRotation(const glm::quat& rot)
        {
                localRotation = rot;
                dirtyTransform = true;
        }

        void setLocalRotationEuler(const glm::vec3& eulerDegrees)
        {
                localRotation = glm::quat(glm::radians(eulerDegrees));
                dirtyTransform = true;
        }

        void setLocalScale(const glm::vec3& scale)
        {
                localScale = scale;
                dirtyTransform = true;
        }

        void addChild(std::shared_ptr<SceneNode> child)
        {
                children.push_back(child);
        }

        void addMesh(std::size_t index)
        {
                meshIndices.push_back(index);
        }

        Transform<NodeSpace, NodeSpace> getLocalTransform()
        {
                if (dirtyTransform) { buildLocalTransformation(); }
                return localTransform;
        }

        Transform<NodeSpace, WorldSpace> getWorldTransform()
        {
                if (dirtyTransform) { buildLocalTransformation(); }

                if (parent) { return localTransform.then(parent->getWorldTransform()); }
                else { return Transform<NodeSpace, WorldSpace>(localTransform.getMatrix()); }
        }

        Point3<WorldSpace> getWorldPosition()
        {
                return getWorldTransform().transformPoint(Point3<NodeSpace>(0.0f, 0.0f, 0.0f));
        }

      private:
        std::uint32_t id;
        std::string name;

        bool dirtyTransform{};

        std::shared_ptr<SceneNode> parent;
        std::vector<std::shared_ptr<SceneNode>> children;

        glm::vec3 localPosition{0.0f};
        glm::quat localRotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 localScale{1.0f};

        std::vector<std::size_t> meshIndices;

        Transform<NodeSpace, NodeSpace> localTransform;

        void buildLocalTransformation()
        {
                localTransform = Transform<NodeSpace, NodeSpace>::fromTRS(localPosition, localRotation, localScale);
                dirtyTransform = false;
        }
};

class SceneGraph
{
      private:
        std::shared_ptr<SceneNode> rootNode;
        std::unordered_map<std::uint32_t, std::shared_ptr<SceneNode>> nodeRegistry;
        std::unordered_map<std::string, std::uint32_t> nodeNameMap;

        std::uint32_t nextID = 0;

      public:
        SceneGraph()
        {
                rootNode = std::make_shared<SceneNode>(nextID++, "root");
                nodeRegistry[rootNode->getID()] = rootNode;
                nodeNameMap["root"] = rootNode->getID();
        }

        const std::shared_ptr<SceneNode> addNode(const std::string& name, std ::shared_ptr<SceneNode> parent = nullptr)
        {
                auto node = std::make_shared<SceneNode>(nextID++, name, parent ? parent : rootNode);

                nodeRegistry[node->getID()] = node;
                nodeNameMap[name] = node->getID();

                if (parent) { parent->addChild(node); }
                else { rootNode->addChild(node); }

                return node;
        }

        std::shared_ptr<SceneNode> getNode(std::uint32_t id) const
        {
                return nodeRegistry.at(id);
        }

        std::shared_ptr<SceneNode> getNode(const std::string& name) const 
        {
                return getNode(nodeNameMap.at(name));
        }

        std::shared_ptr<SceneNode> getRoot() const 
        {
                return rootNode;
        }

        std::vector<std::shared_ptr<SceneNode>> getNodes() const 
        {
                std::vector<std::shared_ptr<SceneNode>> result;

                std::function<void(std::shared_ptr<SceneNode>)> collect = [&](std::shared_ptr<SceneNode> node) {
                        result.push_back(node);
                        for (const auto& child : node->getChildren())
                        {
                                collect(child);
                        }
                };

                collect(rootNode);

                return result;
        }
};
