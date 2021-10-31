#pragma once

#include "sl12/util.h"
#include "render_command.h"


namespace sl12
{
	using RenderCommandPtr = std::unique_ptr<sl12::RenderCommand>;
	using RenderCommandsList = std::vector<RenderCommandPtr>;
	using RenderCommandsTempList = std::vector<sl12::RenderCommand*>;

	//----------------
	class SceneNode
	{
	public:
		SceneNode()
		{}
		virtual ~SceneNode()
		{}

		// create unique render command.
		virtual void CreateRenderCommand(ConstantBufferCache* pCBCache, RenderCommandsList& outRenderCmds)
		{}
	};	// class SceneNode

	using SceneNodePtr = std::shared_ptr<SceneNode>;
	using SceneNodeWeakPtr = std::weak_ptr<SceneNode>;

	//----------------
	class SceneRoot
	{
	public:
		SceneRoot();
		~SceneRoot();

		void AttachNode(SceneNodePtr node);

		void GatherRenderCommands(ConstantBufferCache* pCBCache, RenderCommandsList& outRenderCmds);

		void GabageCollect();

	private:
		std::vector<SceneNodeWeakPtr>	nodes_;
	};	// class SceneRoot

}	// namespace sl12


//	EOF
