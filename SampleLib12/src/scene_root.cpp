#include "sl12/scene_root.h"


namespace sl12
{
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
		nodes_.push_back(node);
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
