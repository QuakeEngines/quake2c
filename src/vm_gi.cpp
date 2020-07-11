#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"
#include "vm_game.h"

static void QC_sound(QCVM &vm)
{
	auto entity = vm.ArgvEntity(0);
	const auto &channel = static_cast<sound_channel_t>(vm.ArgvInt32(1));
	const auto &soundindex = vm.ArgvInt32(2);
	const auto &volume = vm.ArgvFloat(3);
	const auto &attenuation = vm.ArgvFloat(4);
	const auto &timeofs = vm.ArgvFloat(5);

	gi.sound(entity, channel, soundindex, volume, attenuation, timeofs);
}

static void QC_positioned_sound(QCVM &vm)
{
	const auto &position = vm.ArgvVector(0);
	auto entity = vm.ArgvEntity(1);
	const auto &channel = static_cast<sound_channel_t>(vm.ArgvInt32(2));
	const auto &soundindex = vm.ArgvInt32(3);
	const auto &volume = vm.ArgvFloat(4);
	const auto &attenuation = vm.ArgvFloat(5);
	const auto &timeofs = vm.ArgvFloat(6);

	gi.positioned_sound(position, entity, channel, soundindex, volume, attenuation, timeofs);
}

// TODO: move into VM state
struct cvar_handle_t
{
	cvar_t	*cvar;
	string_t name, string, latched_string;
};

static std::vector<cvar_handle_t> cvar_list;

static void QC_cvar_get_name(QCVM &vm)
{
	const auto &cvar = cvar_list[vm.ArgvInt32(0) - 1];
	vm.Return(cvar.name);
}

static void QC_cvar_get_string(QCVM &vm)
{
	const auto &cvar = cvar_list[vm.ArgvInt32(0) - 1];
	vm.Return(cvar.string);
}

static void QC_cvar_get_latched_string(QCVM &vm)
{
	const auto &cvar = cvar_list[vm.ArgvInt32(0) - 1];
	vm.Return(cvar.latched_string);
}

static void QC_cvar_get_modified(QCVM &vm)
{
	const auto &cvar = cvar_list[vm.ArgvInt32(0) - 1];
	vm.Return(cvar.cvar->modified);
}

static void QC_cvar_set_modified(QCVM &vm)
{
	const auto &cvar = cvar_list[vm.ArgvInt32(0) - 1];
	const auto &value = vm.ArgvInt32(1);

	cvar.cvar->modified = static_cast<qboolean>(value);
}

static void QC_cvar_get_floatVal(QCVM &vm)
{
	const auto &cvar = cvar_list[vm.ArgvInt32(0) - 1];
	vm.Return(cvar.cvar->value);
}

static void QC_cvar_get_intVal(QCVM &vm)
{
	const auto &cvar = cvar_list[vm.ArgvInt32(0) - 1];
	vm.Return(static_cast<int32_t>(cvar.cvar->value));
}

static void QC_cvar(QCVM &vm)
{
	const auto &name = vm.ArgvString(0);
	const auto &value = vm.ArgvString(1);
	const auto &flags = static_cast<cvar_flags_t>(vm.ArgvInt32(2));

	const int32_t &val = cvar_list.size() + 1;

	cvar_t *cvar = gi.cvar(name, value, flags);

	cvar_list.push_back({
		.cvar = cvar,
		.name = vm.dynamic_strings.StoreStatic(cvar->name),
		.string = vm.dynamic_strings.StoreStatic(const_cast<const char **>(&cvar->string)),
		.latched_string = vm.dynamic_strings.StoreStatic(const_cast<const char **>(&cvar->latched_string))
	});

	vm.Return(val);
}

static void QC_cvar_set(QCVM &vm)
{
	const auto &name = vm.ArgvString(0);
	const auto &value = vm.ArgvString(1);
	gi.cvar_set(name, value);
}

static void QC_cvar_forceset(QCVM &vm)
{
	const auto &name = vm.ArgvString(0);
	const auto &value = vm.ArgvString(1);
	gi.cvar_set(name, value);
}

static void QC_configstring(QCVM &vm)
{
	const auto &id = static_cast<config_string_t>(vm.ArgvInt32(0));
	const auto &str = vm.ArgvString(1);

	gi.configstring(id, str);
}

static void QC_error(QCVM &vm)
{
	const auto &fmt = vm.ArgvString(0);
	gi.error("%s", ParseFormat(fmt, vm, 1).data());
}

static void QC_modelindex(QCVM &vm)
{
	const auto &str = vm.ArgvString(0);
	vm.Return(gi.modelindex(str));
}

static void QC_soundindex(QCVM &vm)
{
	const auto &str = vm.ArgvString(0);
	vm.Return(gi.soundindex(str));
}

static void QC_imageindex(QCVM &vm)
{
	const auto &str = vm.ArgvString(0);
	vm.Return(gi.imageindex(str));
}

static void QC_setmodel(QCVM &vm)
{
	auto ent = vm.ArgvEntity(0);
	const auto &str = vm.ArgvString(1);

	gi.setmodel(ent, str);
}

enum class QC_csurface_t {};

static void QC_csurface_get_name(QCVM &vm)
{
	auto surf = reinterpret_cast<csurface_t *>(vm.ArgvInt32(0));

	if (!surf)
		vm.Return(string_t::STRING_EMPTY);
	else
		vm.Return(vm.dynamic_strings.StoreStatic(surf->name));
}

static void QC_csurface_get_flags(QCVM &vm)
{
	auto surf = reinterpret_cast<csurface_t *>(vm.ArgvInt32(0));

	if (!surf)
		vm.Return(0);
	else
		vm.Return(surf->flags);
}

static void QC_csurface_get_value(QCVM &vm)
{
	auto surf = reinterpret_cast<csurface_t *>(vm.ArgvInt32(0));

	if (!surf)
		vm.Return(0);
	else
		vm.Return(surf->value);
}

struct QC_trace_t
{
	int				allsolid;
	int				startsolid;
	float			fraction;
	vec3_t			endpos;
	cplane_t		plane;
	QC_csurface_t	surface;
	int				contents;
	ent_t			ent;
};

static trace_t trace_result;

static void QC_trace(QCVM &vm)
{
	const auto &start = vm.ArgvVector(0);
	const auto &mins = vm.ArgvVector(1);
	const auto &maxs = vm.ArgvVector(2);
	const auto &end = vm.ArgvVector(3);
	auto ent = vm.ArgvEntity(4);
	const auto &contents = static_cast<content_flags_t>(vm.ArgvInt32(5));

	trace_result = gi.trace(start, mins, maxs, end, ent, contents);
}

static void QC_trace_result(QCVM &vm)
{
	QC_trace_t out;
	out.allsolid = trace_result.allsolid;
	out.startsolid = trace_result.startsolid;
	out.fraction = trace_result.fraction;
	out.endpos = trace_result.endpos;
	out.plane = trace_result.plane;
	out.surface = static_cast<QC_csurface_t>(reinterpret_cast<int32_t>(trace_result.surface));
	out.contents = trace_result.contents;
	out.ent = vm.EntityToEnt(trace_result.ent);
	vm.SetGlobal<QC_trace_t>(global_t::PARM0, out);
}

static void QC_pointcontents(QCVM &vm)
{
	const auto &pos = vm.ArgvVector(0);
	vm.Return(gi.pointcontents(pos));
}

static void QC_inPVS(QCVM &vm)
{
	const auto &a = vm.ArgvVector(0);
	const auto &b = vm.ArgvVector(1);
	vm.Return(gi.inPVS(a, b));
}

static void QC_inPHS(QCVM &vm)
{
	const auto &a = vm.ArgvVector(0);
	const auto &b = vm.ArgvVector(1);
	vm.Return(gi.inPHS(a, b));
}

static void QC_SetAreaPortalState(QCVM &vm)
{
	const auto &num = vm.ArgvInt32(0);
	const auto &state = static_cast<qboolean>(vm.ArgvInt32(1));

	gi.SetAreaPortalState(num, state);
}

static void QC_AreasConnected(QCVM &vm)
{
	const auto &a = vm.ArgvInt32(0);
	const auto &b = vm.ArgvInt32(1);
	vm.Return(gi.AreasConnected(a, b));
}

static void QC_linkentity(QCVM &vm)
{
	auto ent = vm.ArgvEntity(0);
	gi.linkentity(ent);
}

static void QC_unlinkentity(QCVM &vm)
{
	auto ent = vm.ArgvEntity(0);
	gi.unlinkentity(ent);
}

using QC_box_edicts_t = std::vector<edict_t *>;

static void QC_box_edicts_length(QCVM &vm)
{
	auto *edicts = reinterpret_cast<QC_box_edicts_t *>(vm.ArgvInt32(0));
	vm.Return(static_cast<int32_t>(edicts->size()));
}

static void QC_box_edicts_get(QCVM &vm)
{
	auto *edicts = reinterpret_cast<QC_box_edicts_t *>(vm.ArgvInt32(0));
	const auto &index = vm.ArgvInt32(1);

	if (index < 0 || index >= edicts->size())
		vm.Error("Out of bounds access");

	vm.Return(*edicts->at(index));
}

static void QC_BoxEdicts(QCVM &vm)
{
	const auto &mins = vm.ArgvVector(0);
	const auto &maxs = vm.ArgvVector(1);
	const auto &maxcount = vm.ArgvInt32(2);
	const auto &areatype = static_cast<box_edicts_area_t>(vm.ArgvInt32(3));

	auto edicts = new QC_box_edicts_t(maxcount);
	auto count = gi.BoxEdicts(mins, maxs, edicts->data(), maxcount, areatype);
	edicts->resize(count);
	edicts->shrink_to_fit();

	vm.Return(reinterpret_cast<int32_t>(edicts));

	vm.SetGlobal<int32_t>(global_t::PARM4, edicts->size());
}

static void QC_FreeBoxEdicts(QCVM &vm)
{
	auto *edicts = reinterpret_cast<QC_box_edicts_t *>(vm.ArgvInt32(0));
	delete edicts;
}

struct QC_pmove_state_t
{
    pmtype_t pm_type;

    int	origin[3];		// 12.3
    int	velocity[3];	// 12.3
    int	pm_flags;		// ducked, jump_held, etc
    int	pm_time;		// each unit = 8 ms
    int	gravity;
    int	delta_angles[3];	// add to command angles to get view direction
							// changed by spawns, rotating objects, and teleporters
};

struct QC_pmove_t
{
	// inout
	QC_pmove_state_t	s;
	
	// in
	QC_usercmd_t	cmd;
	bool			snapinitial;
	
	// out
	int		numtouch;
	ent_t	touchents[MAX_TOUCH];
	
	vec3_t	viewangles;
	float	viewheight;
	
	vec3_t	mins, maxs;
	
	ent_t	groundentity;
	int		watertype;
	int		waterlevel;
	
	// in (callbacks)
	func_t trace;
	func_t pointcontents;
};

static func_t QC_pm_pointcontents_func;

static content_flags_t QC_pm_pointcontents(const vec3_t &position)
{
	auto func = qvm.FindFunction(QC_pm_pointcontents_func);
	qvm.SetGlobal<vec3_t>(global_t::PARM0, position);
	qvm.Execute(*func);
	return qvm.GetGlobal<content_flags_t>(global_t::RETURN);
}

static func_t QC_pm_trace_func;

static trace_t QC_pm_trace(const vec3_t &start, const vec3_t &mins, const vec3_t &maxs, const vec3_t &end)
{
	auto func = qvm.FindFunction(QC_pm_trace_func);
	qvm.SetGlobal<vec3_t>(global_t::PARM0, start);
	qvm.SetGlobal<vec3_t>(global_t::PARM1, mins);
	qvm.SetGlobal<vec3_t>(global_t::PARM2, maxs);
	qvm.SetGlobal<vec3_t>(global_t::PARM3, end);
	qvm.Execute(*func);

	return trace_result;
}

static void QC_Pmove(QCVM &vm)
{
	auto pm_from = vm.params_from.at(global_t::PARM0);
	auto qc_pm = vm.GetGlobal<QC_pmove_t>(pm_from);

	pmove_t pm;

	// set in parameters
	pm.s.pm_type = qc_pm.s.pm_type;
	for (int32_t i = 0; i < 3; i++)
	{
		pm.s.origin[i] = qc_pm.s.origin[i];
		pm.s.velocity[i] = qc_pm.s.velocity[i];
		pm.s.delta_angles[i] = qc_pm.s.delta_angles[i];
		pm.cmd.angles[i] = qc_pm.cmd.angles[i];
	}
	pm.s.pm_flags = static_cast<pmflags_t>(qc_pm.s.pm_flags);
	pm.s.pm_time = qc_pm.s.pm_time;
	pm.s.gravity = qc_pm.s.gravity;
	pm.cmd.buttons = static_cast<button_bits_t>(qc_pm.cmd.buttons);
	pm.cmd.forwardmove = qc_pm.cmd.forwardmove;
	pm.cmd.impulse = qc_pm.cmd.impulse;
	pm.cmd.lightlevel = qc_pm.cmd.lightlevel;
	pm.cmd.msec = qc_pm.cmd.msec;
	pm.cmd.sidemove = qc_pm.cmd.sidemove;
	pm.cmd.upmove = qc_pm.cmd.upmove;

	QC_pm_pointcontents_func = qc_pm.pointcontents;
	pm.pointcontents = QC_pm_pointcontents;

	QC_pm_trace_func = qc_pm.trace;
	pm.trace = QC_pm_trace;

	gi.Pmove(&pm);

	// copy out parameters
	qc_pm.s.pm_type = pm.s.pm_type;
	for (int32_t i = 0; i < 3; i++)
	{
		qc_pm.s.origin[i] = pm.s.origin[i];
		qc_pm.s.velocity[i] = pm.s.velocity[i];
		qc_pm.s.delta_angles[i] = pm.s.delta_angles[i];
	}
	qc_pm.s.pm_flags = pm.s.pm_flags;
	qc_pm.s.pm_time = pm.s.pm_time;
	qc_pm.s.gravity = pm.s.gravity;

	qc_pm.numtouch = pm.numtouch;

	for (int32_t i = 0; i < pm.numtouch; i++)
		qc_pm.touchents[i] = vm.EntityToEnt(pm.touchents[i]);

	qc_pm.viewangles = pm.viewangles;
	qc_pm.viewheight = pm.viewheight;
	
	qc_pm.mins = pm.mins;
	qc_pm.maxs = pm.maxs;

	if (!pm.groundentity)
		qc_pm.groundentity = ent_t::ENT_INVALID;
	else
		qc_pm.groundentity = vm.EntityToEnt(pm.groundentity);
	qc_pm.watertype = pm.watertype;
	qc_pm.waterlevel = pm.waterlevel;
	vm.SetGlobal<QC_pmove_t>(pm_from, qc_pm);
}

static void QC_multicast(QCVM &vm)
{
	const auto &pos = vm.ArgvVector(0);
	const auto &type = vm.ArgvInt32(1);

	gi.multicast(pos, static_cast<multicast_t>(type));
}

static void QC_unicast(QCVM &vm)
{
	auto ent = vm.ArgvEntity(0);
	const auto &reliable = vm.ArgvInt32(1);

	gi.unicast(ent, static_cast<qboolean>(reliable));
}

static void QC_WriteChar(QCVM &vm)
{
	const auto &val = vm.ArgvInt32(0);

	gi.WriteChar(val);
}

static void QC_WriteByte(QCVM &vm)
{
	const auto &val = vm.ArgvInt32(0);

	gi.WriteByte(val);
}

static void QC_WriteShort(QCVM &vm)
{
	const auto &val = vm.ArgvInt32(0);

	gi.WriteShort(val);
}

static void QC_WriteLong(QCVM &vm)
{
	const auto &val = vm.ArgvInt32(0);

	gi.WriteLong(val);
}

static void QC_WriteFloat(QCVM &vm)
{
	const auto &val = vm.ArgvFloat(0);

	gi.WriteFloat(val);
}

static void QC_WriteString(QCVM &vm)
{
	const auto &val = vm.ArgvString(0);

	gi.WriteString(val);
}

static void QC_WritePosition(QCVM &vm)
{
	const auto &val = vm.ArgvVector(0);

	gi.WritePosition(val);
}

static void QC_WriteDir(QCVM &vm)
{
	const auto &val = vm.ArgvVector(0);

	gi.WriteDir(val);
}

static void QC_WriteAngle(QCVM &vm)
{
	const auto &val = vm.ArgvFloat(0);

	gi.WriteAngle(val);
}

static void QC_argv(QCVM &vm)
{
	const auto &n = vm.ArgvInt32(0);
	vm.Return(vm.dynamic_strings.StoreStatic(va("%s", gi.argv(n))));
}

static void QC_argc(QCVM &vm)
{
	vm.Return(gi.argc());
}

static void QC_args(QCVM &vm)
{
	vm.Return(vm.dynamic_strings.StoreStatic(va("%s", gi.args())));
}

static void QC_bprintf(QCVM &vm)
{
	const auto &level = static_cast<print_level_t>(vm.ArgvInt32(0));
	const auto &fmt = vm.ArgvString(1);
	gi.bprintf(level, "%s", ParseFormat(fmt, vm, 2).data());
}

static void QC_dprintf(QCVM &vm)
{
	const auto &fmt = vm.ArgvString(0);
	const auto &str = ParseFormat(fmt, vm, 1);
	gi.dprintf("%s", str.data());
}

static void QC_cprintf(QCVM &vm)
{
	auto ent = vm.ArgvEntity(0);
	const auto &level = static_cast<print_level_t>(vm.ArgvInt32(1));
	const auto &fmt = vm.ArgvString(2);
	gi.cprintf(ent, level, "%s", ParseFormat(fmt, vm, 3).data());
}

static void QC_centerprintf(QCVM &vm)
{
	auto ent = vm.ArgvEntity(0);
	const auto &fmt = vm.ArgvString(1);
	gi.centerprintf(ent, "%s", ParseFormat(fmt, vm, 2).data());
}

void InitGIBuiltins(QCVM &vm)
{
	RegisterBuiltin(bprintf);
	RegisterBuiltin(dprintf);
	RegisterBuiltin(cprintf);
	RegisterBuiltin(centerprintf);
	RegisterBuiltin(sound);
	RegisterBuiltin(positioned_sound);
	RegisterBuiltin(cvar);
	RegisterBuiltin(cvar_set);
	RegisterBuiltin(cvar_forceset);

	RegisterBuiltin(configstring);
	
	RegisterBuiltin(error);

	RegisterBuiltin(modelindex);
	RegisterBuiltin(soundindex);
	RegisterBuiltin(imageindex);

	RegisterBuiltin(setmodel);
	
	RegisterBuiltin(trace);
	RegisterBuiltin(trace_result);
	RegisterBuiltin(pointcontents);
	RegisterBuiltin(inPVS);
	RegisterBuiltin(inPHS);
	RegisterBuiltin(SetAreaPortalState);
	RegisterBuiltin(AreasConnected);
	
	RegisterBuiltin(linkentity);
	RegisterBuiltin(unlinkentity);
	RegisterBuiltin(BoxEdicts);
	RegisterBuiltin(FreeBoxEdicts);
	RegisterBuiltin(Pmove);
	
	RegisterBuiltin(multicast);
	RegisterBuiltin(unicast);
	RegisterBuiltin(WriteChar);
	RegisterBuiltin(WriteByte);
	RegisterBuiltin(WriteShort);
	RegisterBuiltin(WriteLong);
	RegisterBuiltin(WriteFloat);
	RegisterBuiltin(WriteString);
	RegisterBuiltin(WritePosition);
	RegisterBuiltin(WriteDir);
	RegisterBuiltin(WriteAngle);
	
	RegisterBuiltin(argv);
	RegisterBuiltin(argc);
	RegisterBuiltin(args);
	
	RegisterBuiltin(box_edicts_length);
	RegisterBuiltin(box_edicts_get);
	
	RegisterBuiltin(csurface_get_name);
	RegisterBuiltin(csurface_get_flags);
	RegisterBuiltin(csurface_get_value);
	
	RegisterBuiltin(cvar_get_name);
	RegisterBuiltin(cvar_get_string);
	RegisterBuiltin(cvar_get_latched_string);
	RegisterBuiltin(cvar_get_modified);
	RegisterBuiltin(cvar_set_modified);
	RegisterBuiltin(cvar_get_floatVal);
	RegisterBuiltin(cvar_get_intVal);
}