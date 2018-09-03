#pragma once
#include "PluginBase/Modules.h"

#include <convar.h>
#include <mathlib/vector.h>
#include <vector>

class C_BaseEntity;
class KeyValues;
class IHandleEntity;
enum ObserverMode;

class AutoCameras final : public Module<AutoCameras>
{
public:
	AutoCameras();
	static constexpr __forceinline const char* GetModuleName() { return "AutoCameras"; }

	static bool CheckDependencies();

	void OnTick(bool ingame) override;

private:
	void LevelInit() override;

	struct StoryboardElement;
	struct Trigger;
	struct Shot;
	struct Action;
	struct Camera;

	void LoadConfig();
	void LoadConfig(const char* mapName);
	bool LoadCameraGroup(KeyValues* cameraGroup);
	bool LoadTrigger(KeyValues* trigger, const char* filename);
	bool LoadCamera(KeyValues* camera, const char* filename);
	bool LoadStoryboard(KeyValues* storyboard, const char* filename);
	bool LoadShot(std::unique_ptr<StoryboardElement>& shot, KeyValues* shotKV, const char* storyboardName, const char* filename);
	bool LoadAction(std::unique_ptr<StoryboardElement>& action, KeyValues* actionKV, const char* storyboardName, const char* filename);

	void CheckTrigger(const Trigger& trigger, std::vector<C_BaseEntity*>& entities);

	void ExecuteStoryboardElement(const StoryboardElement& element, C_BaseEntity* triggerer);
	void ExecuteShot(const Shot& shot, C_BaseEntity* triggerer);
	void ExecuteAction(const Action& action, C_BaseEntity* triggerer);

	void BeginCameraTrigger();
	ConCommand ce_cameratrigger_begin;
	void EndCameraTrigger();
	ConCommand ce_cameratrigger_end;

	void BeginStoryboard(const CCommand& args);
	ConCommand ce_autocamera_begin_storyboard;

	ConCommand ce_autocamera_create;
	void MarkCamera(const CCommand& args);

	ConCommand ce_autocamera_cycle;
	ConVar ce_autocamera_cycle_debug;
	void CycleCamera(const CCommand& args);

	ConCommand ce_autocamera_spec_player;
	ConVar ce_autocamera_spec_player_fallback;
	ConVar ce_autocamera_spec_player_fov;
	ConVar ce_autocamera_spec_player_dist;
	ConVar ce_autocamera_spec_player_los;
	ConVar ce_autocamera_spec_player_debug;
	void SpecPlayer(const CCommand& args);
	ObserverMode GetSpecPlayerFallback() const;
	float ScoreSpecPlayerCamera(const Camera& camera, const Vector& position, const IHandleEntity* targetEnt = nullptr) const;

	ConCommand ce_autocamera_goto;
	ConVar ce_autocamera_goto_mode;
	void GotoCamera(const Camera& camera) { GotoCamera(camera, (ObserverMode)ce_autocamera_goto_mode.GetInt()); }
	void GotoCamera(const Camera& camera, ObserverMode mode);
	void GotoCamera(const CCommand& args);
	static int GotoCameraCompletion(const char *partial, char commands[COMMAND_COMPLETION_MAXITEMS][COMMAND_COMPLETION_ITEM_LENGTH]);

	bool m_CreatingCameraTrigger;
	Vector m_CameraTriggerStart;

	ConCommand ce_autocamera_reload_config;
	ConVar ce_autocamera_show_triggers;
	ConVar ce_autocamera_show_cameras;

	float GetCameraFOV(const Camera& camera, ObserverMode mode) const;

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
	std::vector<std::unique_ptr<const Trigger>> m_Triggers;
	std::vector<std::string> m_MalformedTriggers;
	const Trigger* FindTrigger(const char* triggerName) const;

	struct CameraGroup
	{
		std::string m_Name;

		std::vector<const Camera*> m_Cameras;
	};
	std::vector<std::unique_ptr<const CameraGroup>> m_CameraGroups;
	const CameraGroup* FindCameraGroup(const char* groupName) const;
	void CreateDefaultCameraGroup();

	struct Camera
	{
		std::string m_Name;
		Vector m_Pos;
		QAngle m_DefaultAngle;
		float m_FOV;

		std::string m_MirroredCameraName;
		Camera* m_CameraToMirror;
	};
	std::vector<std::unique_ptr<const Camera>> m_Cameras;
	std::vector<std::string> m_MalformedCameras;
	const Camera* FindCamera(const char* cameraName) const;
	std::vector<const Camera*> GetAlphabeticalCameras() const;

	void SetupMirroredCameras();

	enum class StoryboardElementType
	{
		Shot,
		Action
	};

	struct StoryboardElement
	{
		virtual StoryboardElementType GetType() const = 0;

		const Trigger* m_Trigger = nullptr;
		Target m_Target = Target::Invalid;

		std::unique_ptr<const StoryboardElement> m_Next;
	};

	struct Shot : public StoryboardElement
	{
		StoryboardElementType GetType() const override { return StoryboardElementType::Shot; }

		const Camera* m_Camera = nullptr;
	};

	struct Action : public StoryboardElement
	{
		StoryboardElementType GetType() const override { return StoryboardElementType::Action; }

		enum Effect
		{
			Invalid,
			LerpToPlayer,
		};

		Effect m_Action = Effect::Invalid;
	};

	struct Storyboard
	{
		std::string m_Name;

		std::unique_ptr<const StoryboardElement> m_FirstElement;
	};
	std::vector<std::unique_ptr<const Storyboard>> m_Storyboards;
	std::vector<std::string> m_MalformedStoryboards;

	bool FindContainedString(const std::vector<std::string>& vec, const char* str);

	std::string m_ConfigFilename;

	Vector m_MapOrigin;

	const Camera* m_LastActiveCamera;
	const Storyboard* m_ActiveStoryboard;
	const StoryboardElement* m_ActiveStoryboardElement;
};
