#pragma once

#include "sl12/util.h"
#include "sl12/render_command.h"


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

		// need to call when new frame begin.
		virtual void BeginNewFrame(CommandList* pCmdList)
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
		void DeleteNode(SceneNodePtr node);

		void BeginNewFrame(CommandList* pCmdList);

		void GatherRenderCommands(ConstantBufferCache* pCBCache, RenderCommandsList& outRenderCmds);

		void GabageCollect();

		bool IsDirty() const
		{
			return bDirty_;
		}

	private:
		std::vector<SceneNodeWeakPtr>	nodes_;
		bool							bDirty_;
		bool							bDirty_PrevFrame_;
	};	// class SceneRoot

}	// namespace sl12


//	EOF
