#include "PluginBase/Interfaces.h"
#include "PluginBase/Player.h"
#include "Modules/ExternalMinimap.h"

#include <cdll_int.h>
#include <client/cliententitylist.h>
#include <client/c_world.h>
#include <materialsystem/imaterialsystem.h>
#include <view_shared.h>

#include <Windows.h>

MODULE_REGISTER(ExternalMinimap);

static IBaseClientDLL* s_ClientDLL;
static IClientEntityList* s_ClientEntityList;
static IMaterialSystem* s_MaterialSystem;
static IVEngineClient* s_EngineClient;

static constexpr const char MMAP_NAME[] = "CastingEssentials.ExternalMinimap.Data";

ExternalMinimap::ExternalMinimap() :
	ce_minimap_enabled("ce_minimap_enabled", "0", FCVAR_NONE, "Enables sharing data for the external minimap."),
	ce_minimap_capture("ce_minimap_build", [](const CCommand& cmd) { GetModule()->QueueCapture(cmd); }, "Builds minimap tiles for this map.")
{
	m_SharedMemHandle = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr,
		PAGE_READWRITE, 0, sizeof(MinimapData), MMAP_NAME);

	if (!m_SharedMemHandle)
		Warning(__FUNCTION__ "(): Failed to create the shared memory object\n");

	m_Data = (MinimapData*)MapViewOfFile(m_SharedMemHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MinimapData));
	if (!m_Data)
		Warning(__FUNCTION__ "(): Failed to MapViewOfFile() the shared memory\n");
}

ExternalMinimap::~ExternalMinimap()
{
	if (!UnmapViewOfFile(m_Data))
		Warning(__FUNCTION__ "(): Failed to UnmapViewOfFile() the shared memory!\n");

	if (!CloseHandle(m_SharedMemHandle))
		Warning(__FUNCTION__ "(): Failed to CloseHandle() the shared memory!\n");
}

bool ExternalMinimap::CheckDependencies()
{
	if (!CheckDependency(Interfaces::GetClientDLL(), s_ClientDLL))
		return false;
	if (!CheckDependency(Interfaces::GetClientEntityList(), s_ClientEntityList))
		return false;
	if (!CheckDependency(Interfaces::GetEngineClient(), s_EngineClient))
		return false;
	if (!CheckDependency(Interfaces::GetMaterialSystem(), s_MaterialSystem))
		return false;

	return true;
}

void ExternalMinimap::LevelInit()
{
	if (m_Data)
		strcpy_s(m_Data->m_MapName, s_EngineClient->GetLevelName());
}

static ConVar zx_width("zx_width", "1920");
static ConVar zx_height("zx_height", "1080");
static ConVar zx_ortholeft("zx_ortholeft", "0");
static ConVar zx_orthotop("zx_orthotop", "-10240");
static ConVar zx_orthoright("zx_orthoright", "18204");
static ConVar zx_orthobottom("zx_orthobottom", "0");
static ConVar zx_aspect("zx_aspect", "1.7777778");
static ConVar zx_iterations("zx_iterations", "250");
static ConVar zx_startx("zx_startx", "-8674");
static ConVar zx_endx("zx_endx", "0");
static ConVar zx_starty("zx_starty", "4746");
static ConVar zx_endy("zx_endy", "0");
static ConVar zx_ang("zx_ang", "90");

void ExternalMinimap::RunCapture()
{
	if (!m_CaptureQueued)
		return;

	m_CaptureQueued = false;

	if (!IsInGame())
		return Warning("%s: Must be ingame to capture a minimap.\n", ce_minimap_capture.GetName());

	C_World* world;
	if (auto baseWorld = s_ClientEntityList->GetClientEntity(0); !baseWorld || (world = dynamic_cast<C_World*>(baseWorld->GetBaseEntity())) == nullptr)
		return Warning("%s: Unable to get the world entity.\n", ce_minimap_capture.GetName());

	Warning("%s: Map extents: (%1.2f %1.2f %1.2f) to (%1.2f %1.2f %1.2f)\n", ce_minimap_capture.GetName(),
		XYZ(world->m_WorldMins), XYZ(world->m_WorldMaxs));

	const auto format = ImageFormat::IMAGE_FORMAT_BGRA8888;
	auto fmtSize = ImageLoader::SizeInBytes(format);
	auto memRequired = ImageLoader::GetMemRequired(1024, 1024, 1, format, false);
	auto buf = std::make_unique<unsigned char[]>(memRequired);

	ConVarRef mat_queue_mode("mat_queue_mode");
	if (mat_queue_mode.GetInt() != 0)
		return Warning("%s must be 0 to run %s\n", mat_queue_mode.GetName(), ce_minimap_capture.GetName());

	const auto ITERATIONS = zx_iterations.GetInt();
	for (int i = 0; i < ITERATIONS; i++)
	{
		CViewSetup view;
		memset(&view, 0, sizeof(view));
		//view.origin = world->m_WorldMaxs;
		//Overview: scale 10.00, pos_x -8674, pos_y 4746
		view.origin.Init(
			RemapVal(i, 0, ITERATIONS, zx_startx.GetFloat(), zx_endx.GetFloat()),
			RemapVal(i, 0, ITERATIONS, zx_starty.GetFloat(), zx_endy.GetFloat()),
			world->m_WorldMaxs.z + 5000);
		view.m_flAspectRatio = zx_aspect.GetFloat();

		view.m_bRenderToSubrectOfLargerScreen = true;
		view.x = 0;
		view.y = 0;
		view.width = zx_width.GetFloat();
		view.height = zx_height.GetFloat();

		view.m_bOrtho = true;
		view.m_OrthoLeft = zx_ortholeft.GetFloat();
		view.m_OrthoTop = zx_orthotop.GetFloat();
		view.m_OrthoRight = zx_orthoright.GetFloat();
		view.m_OrthoBottom = zx_orthobottom.GetFloat();

#if false
		view.origin.x = view.origin.y = 0;
		view.m_OrthoLeft = -10000;
		view.m_OrthoRight = 10000;
		view.m_OrthoBottom = -10000;
		view.m_OrthoTop = 10000;
#endif

		view.zNear = 8;
		view.zFar = world->m_WorldMaxs.z - world->m_WorldMins.z + 5100;

		view.angles = QAngle(90, zx_ang.GetFloat(), 0);

		CMatRenderContextPtr pRenderContext(s_MaterialSystem);
		pRenderContext->ClearColor4ub(0, 0, 0, 255);

		s_ClientDLL->RenderView(view, VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH | VIEW_CLEAR_FULL_TARGET, 0);
		s_MaterialSystem->SwapBuffers();

		pRenderContext->ReadPixels(0, 0, 1024, 1024, buf.get(), format);
	}
}

void ExternalMinimap::Update(float frametime)
{
	if (!IsInGame())
		return;

	if (!m_Data)
		return;

	uint8_t newPlayerCount = 0;
	for (auto player : Player::Iterable())
	{
		if (!player->IsAlive())
			continue;

		auto& data = m_Data->m_Players[newPlayerCount++];

		strcpy_s(data.m_Name, player->GetName());

		data.m_Position = player->GetAbsOrigin();
		data.m_Angles = player->GetAbsAngles();

		data.m_Class = (uint8_t)player->GetClass();
		data.m_Team = (uint8_t)player->GetTeam();

		data.m_Health = player->GetHealth();
		data.m_MaxHealth = player->GetMaxHealth();
		data.m_MaxOverheal = player->GetMaxOverheal();
	}

	m_Data->m_PlayerCount = newPlayerCount;

	//RunCapture();
}

void ExternalMinimap::PostRender()
{
	RunCapture();
}

void ExternalMinimap::LevelShutdown()
{
	if (m_Data)
		memset(m_Data->m_MapName, 0, sizeof(m_Data->m_MapName));
}

void ExternalMinimap::QueueCapture(const CCommand& cmd)
{
	m_CaptureQueued = true;
}
