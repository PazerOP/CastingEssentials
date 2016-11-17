#pragma once
#include "PluginBase/Modules.h"

#include <mathlib/vector.h>
#include <convar.h>

#include <vector>

class ConCommand;
class KeyValues;
class C_BaseEntity;

class MapFlythroughs final : public Module
{
public:
	MapFlythroughs();

	static MapFlythroughs* GetModule() { return Modules().GetModule<MapFlythroughs>(); }
	static const char* GetModuleName() { return Modules().GetModuleName<MapFlythroughs>().c_str(); }

	void OnTick(bool ingame) override;

private:
	void LevelInitPreEntity() override;

	struct StoryboardElement;
	struct Trigger;
	struct Shot;
	struct Action;

	void LoadConfig();
	void LoadConfig(const char* mapName);
	bool LoadTrigger(KeyValues* trigger, const char* filename);
	bool LoadCamera(KeyValues* camera, const char* filename);
	bool LoadStoryboard(KeyValues* storyboard, const char* filename);
	bool LoadShot(std::shared_ptr<StoryboardElement>& shot, KeyValues* shotKV, const char* storyboardName, const char* filename);
	bool LoadAction(std::shared_ptr<StoryboardElement>& action, KeyValues* actionKV, const char* storyboardName, const char* filename);

	void CheckTrigger(const std::shared_ptr<Trigger>& trigger, std::vector<C_BaseEntity*>& entities);

	void ExecuteStoryboardElement(const std::shared_ptr<StoryboardElement>& element, C_BaseEntity* triggerer);
	void ExecuteShot(const std::shared_ptr<Shot>& shot, C_BaseEntity* triggerer);
	void ExecuteAction(const std::shared_ptr<Action>& action, C_BaseEntity* triggerer);

	void BeginCameraTrigger();
	ConCommand* ce_cameratrigger_begin;
	void EndCameraTrigger();
	ConCommand* ce_cameratrigger_end;

	void BeginStoryboard(const CCommand& args);
	ConCommand* ce_autocamera_begin_storyboard;

	ConCommand* ce_autocamera_mark_camera;
	void MarkCamera(const CCommand& args);

	ConCommand* ce_autocamera_goto_camera;
	void GotoCamera(const CCommand& args);
	static int GotoCameraCompletion(const char *partial, char commands[COMMAND_COMPLETION_MAXITEMS][COMMAND_COMPLETION_ITEM_LENGTH]);

	bool m_CreatingCameraTrigger;
	Vector m_CameraTriggerStart;

	ConCommand* ce_autocamera_reload_config;

	enum class Target
	{
		Invalid,
		None,
		FirstPlayer,
	};

	struct Trigger
	{
		std::string m_Name;
		Vector m_Mins;
		Vector m_Maxs;
	};
	std::vector<std::shared_ptr<Trigger>> m_Triggers;
	std::shared_ptr<Trigger> FindTrigger(const char* triggerName);
	
	struct Camera
	{
		std::string m_Name;
		Vector m_Pos;
		QAngle m_DefaultAngle;
	};
	std::vector<std::shared_ptr<Camera>> m_Cameras;
	std::shared_ptr<Camera> FindCamera(const char* cameraName);

	struct StoryboardElement
	{
		virtual ~StoryboardElement() = default;
		StoryboardElement()
		{
			m_Target = Target::Invalid;
		}
		std::shared_ptr<Trigger> m_Trigger;
		Target m_Target;

		std::shared_ptr<StoryboardElement> m_Next;
	};

	struct Shot : public StoryboardElement
	{
		virtual ~Shot() = default;
		std::shared_ptr<Camera> m_Camera;
	};

	struct Action : public StoryboardElement
	{
		virtual ~Action() = default;
		enum Effect
		{
			Invalid,
			LerpToPlayer,
		};

		Effect m_Action;
	};

	struct Storyboard
	{
		std::string m_Name;
		std::shared_ptr<StoryboardElement> m_FirstElement;
	};
	std::vector<std::shared_ptr<Storyboard>> m_Storyboards;

	std::shared_ptr<Storyboard> m_ActiveStoryboard;
	std::shared_ptr<StoryboardElement> m_ActiveStoryboardElement;
};