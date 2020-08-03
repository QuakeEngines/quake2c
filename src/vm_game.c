#include "shared/shared.h"
#include "game.h"
#include "g_vm.h"
#include "vm_game.h"

static void QC_SetNumEdicts(qcvm_t *vm)
{
	globals.num_edicts = qcvm_argv_int32(vm, 0);
}

static void QC_ClearEntity(qcvm_t *vm)
{
	edict_t *entity = qcvm_argv_entity(vm, 0);
	const int32_t number = entity->s.number;

	memset(entity, 0, globals.edict_size);
	
	entity->s.number = number;

	if (number > 0 && number <= game.num_clients)
		entity->client = &game.clients[number - 1];

	qcvm_string_list_check_ref_unset(&vm->dynamic_strings, entity, globals.edict_size / sizeof(qcvm_global_t), false);
}

void SyncPlayerState(qcvm_t *vm, edict_t *ent)
{
	for (size_t i = 0; i < vm->fields_size; i++)
	{
		const qcvm_field_wrapper_t *wrap = &vm->field_wraps.wraps[i];

		if (!wrap->field)
			continue;

		const void *field = qcvm_get_entity_field_pointer(ent, i);
		qcvm_field_wrap_list_wrap(&vm->field_wraps, ent, i, field);
	}
}

static void QC_SyncPlayerState(qcvm_t *vm)
{
	edict_t *ent = qcvm_argv_entity(vm, 0);
	SyncPlayerState(vm, ent);
}

const char *ParseSlashes(const char *value)
{
	// no slashes to parse
	if (!strchr(value, '\\'))
		return value;

	// in Q2 this was 128, but just in case...
	// TODO: honestly I'd prefer to make this dynamic in future
	static char slashless_string[MAX_INFO_STRING];

	if (strlen(value) >= sizeof(slashless_string))
		gi.error("string overflow :(((((((");

	const char *src = value;
	char *dst = slashless_string;

	while (*src)
	{
		const char c = *src;

		if (c == '\\')
		{
			const char next = *(src + 1);

			if (!next)
				gi.error("bad string");
			else if (next == '\\' || next == 'n')
			{
				*dst = (next == 'n') ? '\n' : '\\';
				src += 2;
				dst++;

				continue;
			}
		}

		*dst = c;
		src++;
		dst++;
	}

	*dst = 0;
	return slashless_string;
}

static inline void QC_parse_value_into_ptr(qcvm_t *vm, const qcvm_deftype_t type, const char *value, void *ptr)
{
	size_t data_span = 1;

	switch (type)
	{
	case TYPE_STRING:
		*(qcvm_string_t *)ptr = qcvm_store_or_find_string(vm, ParseSlashes(value));
		break;
	case TYPE_FLOAT:
		*(vec_t *)ptr = strtof(value, NULL);
		break;
	case TYPE_VECTOR:
		data_span = 3;
		sscanf(value, "%f %f %f", (vec_t *)ptr, (vec_t *)ptr + 1, (vec_t *)ptr + 2);
		break;
	case TYPE_INTEGER:
		*(int32_t *)ptr = strtol(value, NULL, 10);
		break;
	default:
		qcvm_error(vm, "Couldn't parse field, bad type %i", type);
	}
	
	qcvm_string_list_check_ref_unset(&vm->dynamic_strings, ptr, data_span, false);

	if (type == TYPE_STRING && qcvm_string_list_is_ref_counted(&vm->dynamic_strings, *(qcvm_string_t *)ptr))
		qcvm_string_list_mark_ref_copy(&vm->dynamic_strings, *(qcvm_string_t *)ptr, ptr);
}

static void QC_entity_key_parse(qcvm_t *vm)
{
	edict_t *ent = qcvm_argv_entity(vm, 0);
	const int32_t field = qcvm_argv_int32(vm, 1);
	const char *value = qcvm_argv_string(vm, 2);

	void *ptr = qcvm_get_entity_field_pointer(ent, field);

	qcvm_definition_t *f = vm->field_map_by_id[(qcvm_global_t)field];

	if (!f)
		qcvm_error(vm, "Couldn't match field %i", field);

	QC_parse_value_into_ptr(vm, f->id, value, ptr);
}

static void QC_struct_key_parse(qcvm_t *vm)
{
	const char *struct_name = qcvm_argv_string(vm, 0);
	const char *key_name = qcvm_argv_string(vm, 1);
	const char *value = qcvm_argv_string(vm, 2);
	
	const char *full_name = qcvm_temp_format(vm, "%s.%s", struct_name, key_name);
	qcvm_definition_hash_t *hashed = vm->definition_hashes[Q_hash_string(full_name, vm->definitions_size)];

	for (; hashed; hashed = hashed->hash_next)
		if (!strcmp(qcvm_get_string(vm, hashed->def->name_index), full_name))
			break;

	if (!hashed)
	{
		qcvm_return_int32(vm, 0);
		return;
	}

	qcvm_definition_t *g = hashed->def;
	void *global = qcvm_get_global(vm, g->global_index);
	QC_parse_value_into_ptr(vm, g->id & ~TYPE_GLOBAL, value, global);
	qcvm_return_int32(vm, 1);
}

static void QC_itoe(qcvm_t *vm)
{
	const int32_t number = qcvm_argv_int32(vm, 0);
	qcvm_return_entity(vm, itoe(number));
}

static void QC_etoi(qcvm_t *vm)
{
	const size_t address = qcvm_argv_int32(vm, 0);
	qcvm_return_int32(vm, ((uint8_t *)address - (uint8_t *)globals.edicts) / globals.edict_size);
}

void qcvm_init_game_builtins(qcvm_t *vm)
{
	qcvm_register_builtin(SetNumEdicts);
	qcvm_register_builtin(ClearEntity);
	qcvm_register_builtin(SyncPlayerState);
	
	qcvm_register_builtin(entity_key_parse);
	qcvm_register_builtin(struct_key_parse);

	qcvm_register_builtin(itoe);
	qcvm_register_builtin(etoi);
}