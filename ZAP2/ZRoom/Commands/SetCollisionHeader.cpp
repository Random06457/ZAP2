#include "SetCollisionHeader.h"
#include "../ZRoom.h"
#include "../../ZFile.h"
#include "../../BitConverter.h"
#include "../../StringHelper.h"

using namespace std;

SetCollisionHeader::SetCollisionHeader(ZRoom* nZRoom, std::vector<uint8_t> rawData, int rawDataIndex) : ZRoomCommand(nZRoom, rawData, rawDataIndex)
{
	segmentOffset = SEG2FILESPACE(BitConverter::ToInt32BE(rawData, rawDataIndex + 4));
	collisionHeader = CollisionHeader(nZRoom, rawData, segmentOffset);

	string declaration = "";
	char waterBoxStr[2048];

	if (collisionHeader.waterBoxSegmentOffset != 0)
		sprintf(waterBoxStr, "%s_waterBoxes_%08X", zRoom->GetName().c_str(), collisionHeader.waterBoxSegmentOffset);
	else
		sprintf(waterBoxStr, "0");

	declaration += StringHelper::Sprintf("%i, %i, %i, %i, %i, %i, %i, %s_vertices_%08X, %i, _%s_polygons_%08X, _%s_polygonTypes_%08X, &_%s_camDataList_%08X, %i, %s",
		collisionHeader.absMinX, collisionHeader.absMinY, collisionHeader.absMinZ,
		collisionHeader.absMaxX, collisionHeader.absMaxY, collisionHeader.absMaxZ,
		collisionHeader.numVerts, zRoom->GetName().c_str(), collisionHeader.vtxSegmentOffset, collisionHeader.numPolygons,
		zRoom->GetName().c_str(), collisionHeader.polySegmentOffset, zRoom->GetName().c_str(), collisionHeader.polyTypeDefSegmentOffset,
		zRoom->GetName().c_str(), collisionHeader.camDataSegmentOffset, collisionHeader.numWaterBoxes, waterBoxStr);

	zRoom->parent->AddDeclaration(segmentOffset, DeclarationAlignment::None, DeclarationPadding::Pad16, 44, "CollisionHeader", 
		StringHelper::Sprintf("_%s_collisionHeader_%08X", zRoom->GetName().c_str(), segmentOffset), declaration);
}

string SetCollisionHeader::GenerateSourceCodePass1(string roomName, int baseAddress)
{
	return StringHelper::Sprintf("%s 0x00, (u32)&_%s_collisionHeader_%08X", ZRoomCommand::GenerateSourceCodePass1(roomName, baseAddress).c_str(), zRoom->GetName().c_str(), segmentOffset);
}

string SetCollisionHeader::GenerateSourceCodePass2(string roomName, int baseAddress)
{
	return "";
}

string SetCollisionHeader::GetCommandCName()
{
	return "SCmdColHeader";
}

string SetCollisionHeader::GenerateExterns()
{
	return StringHelper::Sprintf("extern CollisionHeader _%s_collisionHeader_%08X;\n", zRoom->GetName().c_str(), segmentOffset);
}

RoomCommand SetCollisionHeader::GetRoomCommand()
{
	return RoomCommand::SetCollisionHeader;
}

CollisionHeader::CollisionHeader()
{

}

CollisionHeader::CollisionHeader(ZRoom* zRoom, std::vector<uint8_t> rawData, int rawDataIndex)
{
	uint8_t* data = rawData.data();

	absMinX = BitConverter::ToInt16BE(data, rawDataIndex + 0);
	absMinY = BitConverter::ToInt16BE(data, rawDataIndex + 2);
	absMinZ = BitConverter::ToInt16BE(data, rawDataIndex + 4);

	absMaxX = BitConverter::ToInt16BE(data, rawDataIndex + 6);
	absMaxY = BitConverter::ToInt16BE(data, rawDataIndex + 8);
	absMaxZ = BitConverter::ToInt16BE(data, rawDataIndex + 10);

	numVerts = BitConverter::ToInt16BE(data, rawDataIndex + 12);
	vtxSegmentOffset = BitConverter::ToInt32BE(data, rawDataIndex + 16) & 0x00FFFFFF;

	numPolygons = BitConverter::ToInt16BE(data, rawDataIndex + 20);
	polySegmentOffset = BitConverter::ToInt32BE(data, rawDataIndex + 24) & 0x00FFFFFF;
	polyTypeDefSegmentOffset = BitConverter::ToInt32BE(data, rawDataIndex + 28) & 0x00FFFFFF;
	camDataSegmentOffset = BitConverter::ToInt32BE(data, rawDataIndex + 32) & 0x00FFFFFF;

	numWaterBoxes = BitConverter::ToInt16BE(data, rawDataIndex + 36);
	waterBoxSegmentOffset = BitConverter::ToInt32BE(data, rawDataIndex + 40) & 0x00FFFFFF;

	// HOTSPOT
	for (int i = 0; i < numVerts; i++)
		vertices.push_back(new VertexEntry(zRoom, rawData, vtxSegmentOffset + (i * 6)));

	// HOTSPOT
	for (int i = 0; i < numPolygons; i++)
		polygons.push_back(new PolygonEntry(zRoom, rawData, polySegmentOffset + (i * 16)));

	int highestPolyType = 0;

	for (PolygonEntry* poly : polygons)
	{
		if (poly->type > highestPolyType)
			highestPolyType = poly->type;
	}

	//if (highestPolyType > 0)
	{
		for (int i = 0; i < highestPolyType + 1; i++)
			polygonTypes.push_back(BitConverter::ToInt64BE(data, polyTypeDefSegmentOffset + (i * 8)));
	}
	//else
	//{
		//int polyTypesSize = abs(polyTypeDefSegmentOffset - camDataSegmentOffset) / 8;

		//for (int i = 0; i < polyTypesSize; i++)
			//polygonTypes.push_back(BitConverter::ToInt64BE(data, polyTypeDefSegmentOffset + (i * 8)));
	//}

	if (camDataSegmentOffset != 0)
		camData = new CameraDataList(zRoom, rawData, camDataSegmentOffset, polyTypeDefSegmentOffset, polygonTypes.size());

	for (int i = 0; i < numWaterBoxes; i++)
		waterBoxes.push_back(new WaterBoxHeader(zRoom, rawData, waterBoxSegmentOffset + (i * 16)));

	string declaration = "";
	char line[2048];

	if (waterBoxes.size() > 0)
	{
		for (int i = 0; i < waterBoxes.size(); i++)
		{
			sprintf(line, "\t{ %i, %i, %i, %i, %i, 0x%08X },\n", waterBoxes[i]->xMin, waterBoxes[i]->ySurface, waterBoxes[i]->zMin, waterBoxes[i]->xLength, waterBoxes[i]->zLength, waterBoxes[i]->properties);
			declaration += line;
		}
	}

	if (waterBoxSegmentOffset != 0)
		zRoom->parent->declarations[waterBoxSegmentOffset] = new Declaration(DeclarationAlignment::None, 16 * waterBoxes.size(), "WaterBox",
			StringHelper::Sprintf("%s_waterBoxes_%08X", zRoom->GetName().c_str(), waterBoxSegmentOffset), true, declaration);

	if (polygons.size() > 0)
	{
		declaration = "";

		for (int i = 0; i < polygons.size(); i++)
		{
			sprintf(line, "\t{ 0x%04X, 0x%04X, 0x%04X, 0x%04X, 0x%04X, 0x%04X, 0x%04X, 0x%04X }, // 0x%08X\n",
				(uint16_t)polygons[i]->type, (uint16_t)polygons[i]->vtxA, (uint16_t)polygons[i]->vtxB, (uint16_t)polygons[i]->vtxC,
				(uint16_t)polygons[i]->a, (uint16_t)polygons[i]->b, (uint16_t)polygons[i]->c, (uint16_t)polygons[i]->d, polySegmentOffset + (i * 16));
			declaration += line;
		}

		if (polySegmentOffset != 0) {
			zRoom->parent->declarations[polySegmentOffset] = new Declaration(DeclarationAlignment::None, polygons.size() * 16, "RoomPoly", // TODO: Change this to CollisionPoly once the struct has been updated
				StringHelper::Sprintf("_%s_polygons_%08X", zRoom->GetName().c_str(), polySegmentOffset), true, declaration);
		}
	}

	declaration = "";
	for (int i = 0; i < polygonTypes.size(); i++)
	{
		sprintf(line, "\t 0x%08lX, 0x%08lX, \n",  polygonTypes[i] >> 32, polygonTypes[i] & 0xFFFFFFFF);
		declaration += line;
	}

	if (polyTypeDefSegmentOffset != 0)
		zRoom->parent->declarations[polyTypeDefSegmentOffset] = new Declaration(DeclarationAlignment::None, polygonTypes.size() * 8, 
			"u32", StringHelper::Sprintf("_%s_polygonTypes_%08X", zRoom->GetName().c_str(), polyTypeDefSegmentOffset), true, declaration);

	declaration = "";

	if (vertices.size() > 0)
	{
		declaration = "";

		for (int i = 0; i < vertices.size(); i++)
		{
			sprintf(line, "\t{ %i, %i, %i }, // 0x%08X\n", vertices[i]->x, vertices[i]->y, vertices[i]->z, vtxSegmentOffset + (i * 6));
			declaration += line;
		}

		if (vtxSegmentOffset != 0)
			zRoom->parent->declarations[vtxSegmentOffset] = new Declaration(DeclarationAlignment::None, vertices.size() * 6,
				"Vec3s", StringHelper::Sprintf("%s_vertices_%08X", zRoom->GetName().c_str(), vtxSegmentOffset), true, declaration);

		declaration = "";
	}
}

PolygonEntry::PolygonEntry(ZRoom* zRoom, std::vector<uint8_t> rawData, int rawDataIndex)
{
	uint8_t* data = rawData.data();

	type = BitConverter::ToInt16BE(data, rawDataIndex + 0);
	vtxA = BitConverter::ToInt16BE(data, rawDataIndex + 2);
	vtxB = BitConverter::ToInt16BE(data, rawDataIndex + 4);
	vtxC = BitConverter::ToInt16BE(data, rawDataIndex + 6);
	a = BitConverter::ToInt16BE(data, rawDataIndex + 8);
	b = BitConverter::ToInt16BE(data, rawDataIndex + 10);
	c = BitConverter::ToInt16BE(data, rawDataIndex + 12);
	d = BitConverter::ToInt16BE(data, rawDataIndex + 14);
}

VertexEntry::VertexEntry(ZRoom* zRoom, std::vector<uint8_t> rawData, int rawDataIndex)
{
	uint8_t* data = rawData.data();

	x = BitConverter::ToInt16BE(data, rawDataIndex + 0);
	y = BitConverter::ToInt16BE(data, rawDataIndex + 2);
	z = BitConverter::ToInt16BE(data, rawDataIndex + 4);
}

WaterBoxHeader::WaterBoxHeader(ZRoom* zRoom, std::vector<uint8_t> rawData, int rawDataIndex)
{
	uint8_t* data = rawData.data();

	xMin = BitConverter::ToInt16BE(data, rawDataIndex + 0);
	ySurface = BitConverter::ToInt16BE(data, rawDataIndex + 2);
	zMin = BitConverter::ToInt16BE(data, rawDataIndex + 4);
	xLength = BitConverter::ToInt16BE(data, rawDataIndex + 6);
	zLength = BitConverter::ToInt16BE(data, rawDataIndex + 8);
	properties = BitConverter::ToInt32BE(data, rawDataIndex + 12);
}

CameraDataList::CameraDataList(ZRoom* zRoom, std::vector<uint8_t> rawData, int rawDataIndex, int polyTypeDefSegmentOffset, int polygonTypesCnt)
{
	string declaration = "";

	// Parse CameraDataEntries
	int numElements = abs(polyTypeDefSegmentOffset - (rawDataIndex)) / 8;
	//int numElements = polygonTypesCnt;
	uint32_t cameraPosDataSeg = rawDataIndex;
	for (int i = 0; i < numElements; i++)
	{
		CameraDataEntry* entry = new CameraDataEntry();

		entry->cameraSType = BitConverter::ToInt16BE(rawData, rawDataIndex + (entries.size() * 8) + 0);
		entry->numData = BitConverter::ToInt16BE(rawData, rawDataIndex + (entries.size() * 8) + 2);
		entry->cameraPosDataSeg = BitConverter::ToInt32BE(rawData, rawDataIndex + (entries.size() * 8) + 4);

		if (entry->cameraPosDataSeg != 0 && GETSEGNUM(entry->cameraPosDataSeg) != 2)
		{
			cameraPosDataSeg = rawDataIndex + (entries.size() * 8);
			break;
		}

		if (entry->cameraPosDataSeg != 0 && cameraPosDataSeg > (entry->cameraPosDataSeg & 0xFFFFFF))
			cameraPosDataSeg = (entry->cameraPosDataSeg & 0xFFFFFF);

		entries.push_back(entry);
	}

	//setting cameraPosDataAddr to rawDataIndex give a pos list length of 0
	uint32_t cameraPosDataOffset = cameraPosDataSeg & 0xFFFFFF;
	for (int i = 0; i < entries.size(); i++)
	{
		char camSegLine[2048];

		if (entries[i]->cameraPosDataSeg != 0)
		{
			int index = ((entries[i]->cameraPosDataSeg & 0x00FFFFFF) - cameraPosDataOffset) / 0x6;
			sprintf(camSegLine, "&_%s_camPosData_%08X[%i]", zRoom->GetName().c_str(), cameraPosDataOffset, index);
		}
		else
			sprintf(camSegLine, "0x%08X", entries[i]->cameraPosDataSeg);

		declaration += StringHelper::Sprintf("\t{ 0x%04X, %i, %s }, // 0x%08X\n", entries[i]->cameraSType, entries[i]->numData, camSegLine, rawDataIndex + (i * 8));
	}

	zRoom->parent->AddDeclarationArray(rawDataIndex, DeclarationAlignment::None, entries.size() * 8, "CamData", StringHelper::Sprintf("_%s_camDataList_%08X", zRoom->GetName().c_str(), rawDataIndex), entries.size(), declaration);

	int numDataTotal = abs(rawDataIndex - (int)cameraPosDataOffset) / 0x6;

	if (numDataTotal > 0)
	{
		declaration = "";
		for (int i = 0; i < numDataTotal; i++)
		{
			CameraPositionData* data = new CameraPositionData(zRoom, rawData, cameraPosDataOffset + (i * 6));
			cameraPositionData.push_back(data);

			declaration += StringHelper::Sprintf("\t{ %6i, %6i, %6i }, // 0x%08X\n", data->x, data->y, data->z, cameraPosDataSeg + (i * 0x6));;
		}

		int cameraPosDataIndex = cameraPosDataSeg & 0x00FFFFFF;
		int entrySize = numDataTotal * 0x6;
		zRoom->parent->AddDeclarationArray(cameraPosDataIndex, DeclarationAlignment::None, entrySize, "Vec3s", StringHelper::Sprintf("_%s_camPosData_%08X", zRoom->GetName().c_str(), cameraPosDataIndex), numDataTotal, declaration);
	}
}

CameraPositionData::CameraPositionData(ZRoom* zRoom, std::vector<uint8_t> rawData, int rawDataIndex)
{
	x = BitConverter::ToInt16BE(rawData, rawDataIndex + 0);
	y = BitConverter::ToInt16BE(rawData, rawDataIndex + 2);
	z = BitConverter::ToInt16BE(rawData, rawDataIndex + 4);
}