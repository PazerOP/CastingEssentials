#pragma once
#include "PluginBase/Modules.h"

#include <mathlib/vector.h>
#include <convar.h>

#include <vector>

class ConCommand;
class KeyValues;
class C_BaseEntity;

class MapFlythroughs final : public Module<MapFlythroughs>
{
public:
	MapFlythroughs();

	void OnTick(bool ingame) override;

private:
	void LevelInitPreEntity() override;

	struct StoryboardElement;
	struct Trigger;
	struct Shot;
	struct Action;
	struct Camera;

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

	ConCommand* ce_autocamera_cycle;
	void CycleCamera(const CCommand& args);

	ConCommand* ce_autocamera_goto_camera;
	void GotoCamera(const std::shared_ptr<Camera>& camera);
	void GotoCamera(const CCommand& args);
	static int GotoCameraCompletion(const char *partial, char commands[COMMAND_COMPLETION_MAXITEMS][COMMAND_COMPLETION_ITEM_LENGTH]);

	bool m_CreatingCameraTrigger;
	Vector m_CameraTriggerStart;

	ConCommand* ce_autocamera_reload_config;
	ConVar* ce_autocamera_show_triggers;
	ConVar* ce_autocamera_show_cameras;

	void DrawTriggers();
	void DrawCameras();

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
	std::vector<std::string> m_MalformedTriggers;
	std::shared_ptr<Trigger> FindTrigger(const char* triggerName);

	struct Camera
	{
		std::string m_Name;
		Vector m_Pos;
		QAngle m_DefaultAngle;

		std::string m_MirroredCameraName;
		Camera* m_CameraToMirror;
	};
	std::vector<std::shared_ptr<Camera>> m_Cameras;
	std::vector<std::string> m_MalformedCameras;
	std::shared_ptr<Camera> FindCamera(const char* cameraName);
	std::vector<std::shared_ptr<const Camera>> GetAlphabeticalCameras() const;

	void SetupMirroredCameras();

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
	std::vector<std::string> m_MalformedStoryboards;

	bool FindContainedString(const std::vector<std::string>& vec, const char* str);

	std::string m_ConfigFilename;

	Vector m_MapOrigin;

	std::shared_ptr<Storyboard> m_ActiveStoryboard;
	std::shared_ptr<StoryboardElement> m_ActiveStoryboardElement;
};