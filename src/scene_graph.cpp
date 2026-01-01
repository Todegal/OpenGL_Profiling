#include "scene_graph.h"

SceneGraph::SceneGraph(const RawScene& rawScene)
{
        const auto nodeIndex = nodes.size();

        rootNode = std::make_shared<SceneNode>("root");
        indexMap["root"] = nodeIndex;
        nodes.push_back(rootNode);

        const auto& sceneNodes = rawScene.getNodes();
        if (sceneNodes.size() == 0) { return; }

        std::vector<std::shared_ptr<SceneNode>> tempNodes;
        tempNodes.reserve(sceneNodes.size());

        for (const auto& sceneNode : sceneNodes)
        {
                const auto parentIdx = sceneNode->getParentIdx();

                const auto nodePtr =
                    addNode(sceneNode->getName(), parentIdx.has_value() ? tempNodes[parentIdx.value()] : nullptr);
                tempNodes.push_back(nodePtr);

                for (const auto& meshIdx : sceneNode->getMeshIndices())
                {
                        nodePtr->addMesh(meshIdx);
                }

                nodePtr->setLocalTranslation(sceneNode->getTranslation());
                nodePtr->setLocalRotation(sceneNode->getRotation());
                nodePtr->setLocalScale(sceneNode->getScale());
        }
}

std::shared_ptr<SceneNode> SceneGraph::addNode(const std::string& name, std::shared_ptr<SceneNode> parent)
{
        const auto nodeIndex = nodes.size();

        auto node = std::make_shared<SceneNode>(name, parent ? parent : rootNode);
        nodes.push_back(node);
        indexMap[name] = nodeIndex;

        if (parent) { parent->addChild(node); }
        else { rootNode->addChild(node); }

        return node;
}
