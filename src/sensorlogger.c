#include <stdio.h>
#include <stdlib.h>
#include <base64.h>
#include <cJSON.h>
#include <sensorlogger.h>

void slogger_http_request_release(struct slogger_http_request *rq)
{
	free(rq->url);
	free(rq->data);
	free(rq->headers);
	free(rq);
}

#define APPEND(r, object, field) \
	if (object->field) \
		cJSON_AddItemToObject(r, #field, cJSON_CreateString(object->field)); \
	else \
		cJSON_AddItemToObject(r, #field, cJSON_CreateString(""));


static char *slogger_get_headers(struct slogger_instance *inst)
{
	char *ret;
	char *cred;
	char *base64_cred;

	int len = asprintf(&cred, "%s:%s", inst->userName, inst->password);

	base64_cred = malloc(b64e_size(len) + 1);
	b64_encode(cred, len, base64_cred);

	asprintf(&ret,
		 "Content-Type: application/json\r\n"
		 "Authorization: Basic %s\r\n",
		 base64_cred
		 );
	free(cred);
	free(base64_cred);
	return ret;
}

struct slogger_http_request *slogger_instance_rq_register(struct slogger_instance *inst)
{
	struct slogger_http_request *ret;
	cJSON *root, *dt;

	root = cJSON_CreateObject();

	APPEND(root, inst, deviceId);
	APPEND(root, inst, deviceName);
	APPEND(root, inst, deviceType);
	APPEND(root, inst, deviceGroup);
	APPEND(root, inst, deviceParentGroup);


	cJSON_AddItemToObject(root, "deviceDataTypes", dt = cJSON_CreateArray());
	int i = 0;
	while (inst->deviceDataTypes[i].type) {
		cJSON *tmp;
		tmp = cJSON_CreateObject();
		struct slogger_data_type *current = &inst->deviceDataTypes[i];
		APPEND(tmp, current, type);
		APPEND(tmp, current, description);
		APPEND(tmp, current, unit);
		cJSON_AddItemToArray(dt, tmp);
		i++;
	}

	ret = malloc(sizeof(*ret));
	ret->userdata = NULL;	
	ret->data = cJSON_Print(root);
	ret->headers = slogger_get_headers(inst);
	asprintf(&ret->url, "%s/index.php/apps/sensorlogger/api/v1/registerdevice/", inst->nextCloudUrl);
	cJSON_Delete(root);
	return ret;
}


void float2char(float val, char *smallBuff)
{
	int val1 = (int)val;
	unsigned int val2;

	if (val < 0)
		val2 = (int)(-100.0 * val) % 100;
	else
		val2 = (int)(100.0 * val) % 100;
	if (val2 < 10)
		sprintf(smallBuff, "%i.0%u", val1, val2);
	else
		sprintf(smallBuff, "%i.%u", val1, val2);
}

struct slogger_http_request *slogger_instance_rq_post(struct slogger_instance *inst)
{
	struct slogger_http_request *ret;
	cJSON *root, *dt, *item;

	root = cJSON_CreateObject();
	APPEND(root, inst, deviceId);

	cJSON_AddItemToObject(root, "data", dt = cJSON_CreateArray());

	int i = 0;
	while (inst->deviceDataTypes[i].type) {
		struct slogger_data_type *current = &inst->deviceDataTypes[i];
		char tmp[64];
		item = cJSON_CreateObject();
		cJSON_AddItemToObject(item, "dataTypeId", cJSON_CreateNumber(current->dataTypeId));
		float2char(current->get_current_value(current), tmp);
		cJSON_AddStringToObject(item, "value", tmp);
		cJSON_AddItemToArray(dt, item);
		i++;
	}

	ret = malloc(sizeof(*ret));
	ret->userdata = NULL;
	ret->data = cJSON_Print(root);
	ret->headers = slogger_get_headers(inst);
	asprintf(&ret->url, "%s/index.php/apps/sensorlogger/api/v1/createlog/", inst->nextCloudUrl);
	cJSON_Delete(root);
	return ret;
}

struct slogger_http_request *slogger_instance_rq_get_data_types(struct slogger_instance *inst)
{
	struct slogger_http_request *ret;
	cJSON *root, *dt, *item;

	root = cJSON_CreateObject();
	APPEND(root, inst, deviceId);

	ret = malloc(sizeof(*ret));
	ret->userdata = NULL;
	ret->data = cJSON_Print(root);
	ret->headers = slogger_get_headers(inst);
	asprintf(&ret->url, "%s/index.php/apps/sensorlogger/api/v1/getdevicedatatypes/", inst->nextCloudUrl);
	cJSON_Delete(root);
	return ret;
}


void slogger_instance_set_data_type_id(struct slogger_instance *inst,
				       char *			description,
				       char *			type,
				       char *			shortd,
				       int			id
				       )
{
	struct slogger_data_type *pos = inst->deviceDataTypes;
	while(pos->type) {
		if ((strcmp(pos->type, type) == 0) &&
			(strcmp(pos->description, description) == 0) &&
			(strcmp(pos->unit, shortd) == 0)) {
				pos->dataTypeId = id;
			}
		pos++;
	}
}

void slogger_instance_populate_data_type_ids(struct slogger_instance *inst, char *json)
{
	cJSON *tmp = cJSON_Parse(json);
	cJSON *obj;

	if (!tmp) {
		return;
	}

	if (tmp->type != cJSON_Array) {
		goto bailout;
	}

	obj = tmp->child;
	while (obj) {
		if (obj->type != cJSON_Object) {
			goto bailout;
		}

		cJSON *prop = obj->child;
		int id = 0;
		char *description = NULL;
		char *type = NULL;
		char *shortd = NULL;
		while (prop) {
			if (strcmp(prop->string, "id") == 0)
				id = atoi(prop->valuestring);
			else if (strcmp(prop->string, "description") == 0)
				description = prop->valuestring;
			else if (strcmp(prop->string, "type") == 0)
				type = prop->valuestring;
			else if (strcmp(prop->string, "short") == 0)
				shortd = prop->valuestring;
			prop = prop->next;
		}

		slogger_instance_set_data_type_id(inst, description, type, shortd, id);
		obj = obj->next;
	}

bailout:
	cJSON_Delete(tmp);
}
