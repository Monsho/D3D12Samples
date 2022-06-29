#include "sl12/scene_root.h"


namespace sl12
{
	namespace
	{
		std::vector<SceneNodeWeakPtr>::iterator FindNode(std::vector<SceneNodeWeakPtr>& nodes, SceneNodePtr node)
		{
			return std::find_if(
				nodes.begin(), nodes.end(),
				[node](const SceneNodeWeakPtr& n) { return n.lock().get() == node.get(); });
		}
	}

	//----------------
	//----
	SceneRoot::SceneRoot()
	{}

	//----
	SceneRoot::~SceneRoot()
	{
		nodes_.clear();
	}

	//----
	void SceneRoot::AttachNode(SceneNodePtr node)
	{
		auto exist = FindNode(nodes_, node);
		if (exist != nodes_.end())
		{
			return;
		}

		nodes_.push_back(node);
		bDirty_PrevFrame_ = true;
	}

	//----
	void SceneRoot::DeleteNode(SceneNodePtr node)
	{
		auto exist = FindNode(nodes_, node);
		if (exist == nodes_.end())
		{
			return;
		}

		nodes_.erase(exist);
		bDirty_PrevFrame_ = true;
	}

	//----
	void SceneRoot::BeginNewFrame(CommandList* pCmdList)
	{
		bDirty_ = bDirty_PrevFrame_;
		bDirty_PrevFrame_ = false;
		for (auto&& node : nodes_)
		{
			if (auto r = node.lock())
			{
				r->BeginNewFrame(pCmdList);
			}
		}
	}

	//----
	void SceneRoot::GatherRenderCommands(ConstantBufferCache* pCBCache, RenderCommandsList& outRenderCmds)
	{
		for (auto&& node : nodes_)
		{
			if (auto r = node.lock())
			{
				r->CreateRenderCommand(pCBCache, outRenderCmds);
			}
		}
	}

	//----
	void SceneRoot::GabageCollect()
	{
		auto it = nodes_.begin();
		while (it != nodes_.end())
		{
			if (it->expired())
			{
				it = nodes_.erase(it);
			}
		}
	}

}	// namespace sl12


//	EOF
