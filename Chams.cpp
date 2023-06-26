// Don't take credits for this ;) Joplin / Manhhao are the first uploaders ;)

#include "Chams.h"
#include "offsets.h"
#include "SDK.h"
#include "Interfaces.h"

using GetSymbolProcFn = bool(__cdecl*)(const char*);

#define INVALID_KEY_SYMBOL (-1)
using HKeySymbol = int;

class IKeyValuesSystem
{
public:
	virtual void RegisterSizeofKeyValues(int iSize) = 0;
private:
	virtual void function0() = 0;
public:
	virtual void* AllocKeyValuesMemory(int iSize) = 0;
	virtual void FreeKeyValuesMemory(void* pMemory) = 0;
	virtual HKeySymbol GetSymbolForString(const char* szName, bool bCreate = true) = 0;
	virtual const char* GetStringForSymbol(HKeySymbol hSymbol) = 0;
	virtual void AddKeyValuesToMemoryLeakList(void* pMemory, HKeySymbol hSymbolName) = 0;
	virtual void RemoveKeyValuesFromMemoryLeakList(void* pMemory) = 0;
	virtual void SetKeyValuesExpressionSymbol(const char* szName, bool bValue) = 0;
	virtual bool GetKeyValuesExpressionSymbol(const char* szName) = 0;
	virtual HKeySymbol GetSymbolForStringCaseSensitive(HKeySymbol& hCaseInsensitiveSymbol, const char* szName, bool bCreate = true) = 0;
};

IKeyValuesSystem* KeyValuesSystem;
void InitKeyvalues()
{
	if (const HINSTANCE handle = GetModuleHandle("vstdlib.dll"))
		KeyValuesSystem = reinterpret_cast<IKeyValuesSystem * (__cdecl*)()>(GetProcAddress(handle, "KeyValuesSystem"))();
}

class CKeyValues
{
public:
	enum EKeyType : int
	{
		TYPE_NONE = 0,
		TYPE_STRING,
		TYPE_INT,
		TYPE_FLOAT,
		TYPE_PTR,
		TYPE_WSTRING,
		TYPE_COLOR,
		TYPE_UINT64,
		TYPE_COMPILED_INT_BYTE,
		TYPE_COMPILED_INT_0,
		TYPE_COMPILED_INT_1,
		TYPE_NUMTYPES
	};

	CKeyValues(const char* szKeyName, void* pUnknown1 = nullptr, int hCaseInsensitiveKeyName = -1);
	~CKeyValues();

	void* operator new(std::size_t nAllocSize);
	void operator delete(void* pMemory);

	const char* GetName();

	static CKeyValues* FromString(const char* szName, const char* szValue);
	void LoadFromBuffer(char const* szResourceName, const char* szBuffer, void* pFileSystem = nullptr, const char* szPathID = nullptr, GetSymbolProcFn pfnEvaluateSymbolProc = nullptr);
	bool LoadFromFile(void* pFileSystem, const char* szResourceName, const char* szPathID = nullptr, GetSymbolProcFn pfnEvaluateSymbolProc = nullptr);

	CKeyValues* FindKey(const char* szKeyName, const bool bCreate);

	int GetInt(const char* szKeyName, const int iDefaultValue);
	float GetFloat(const char* szKeyName, const float flDefaultValue);
	const char* GetString(const char* szKeyName, const char* szDefaultValue);

	void SetString(const char* szKeyName, const char* szStringValue);
	void SetInt(const char* szKeyName, const int iValue);
	void SetUint64(const char* szKeyName, const int nLowValue, const int nHighValue);

	inline void SetBool(const char* szKeyName, const bool bValue)
	{
		SetInt(szKeyName, bValue ? 1 : 0);
	}

public:
	std::uint32_t uKeyName : 24; // 0x00
	std::uint32_t uKeyNameCaseSensitive1 : 8; // 0x3 // byte, explicitly specify bits due to packing
	char* szValue; // 0x04
	wchar_t* wszValue; // 0x08

	union
	{
		int iValue;
		float flValue;
		void* pValue;
		unsigned char arrColor[4];
	}; // 0x0C

	std::int8_t iDataType; // 0x10
	bool bHasEscapeSequences; // 0x11
	std::uint16_t uKeyNameCaseSensitive2; // 0x12
	void* pUnknown14; // 0x14 // seems like IKeyValuesSystem*, but why do they need it here? also calling smth on destructor and cleans up
	bool bHasCaseInsensitiveKeySymbol; // 0x18
	CKeyValues* pPeer; // 0x1C
	CKeyValues* pSub; // 0x20
	CKeyValues* pChain; // 0x24
	GetSymbolProcFn	pExpressionGetSymbolProc; // 0x28
};

CKeyValues::CKeyValues(const char* szKeyName, void* pUnknown1, int hCaseInsensitiveKeyName)
{
	using CKeyValuesConstructorFn = void(__thiscall*)(void*, const char*, void*, int);
	static CKeyValuesConstructorFn oConstructor = reinterpret_cast<CKeyValuesConstructorFn>(Utilities::Memory::FindPatternV2("client.dll", "55 8B EC 56 8B F1 33 C0 8B 4D 0C 81")); // @xref: client.dll -> "OldParticleSystem_Destroy"
	oConstructor(this, szKeyName, pUnknown1, hCaseInsensitiveKeyName);
}

CKeyValues::~CKeyValues()
{
	using CKeyValuesDestructorFn = void(__thiscall*)(void*, int);
	static CKeyValuesDestructorFn oDestructor = reinterpret_cast<CKeyValuesDestructorFn>(Utilities::Memory::FindPatternV2("client.dll", "56 8B F1 E8 ? ? ? ? 8B 4E 14"));
	oDestructor(this, 1);
}

void* CKeyValues::operator new(std::size_t nAllocSize)
{
	// manually allocate memory, because game constructor doesn't call it automatically
	return KeyValuesSystem->AllocKeyValuesMemory(nAllocSize);
}

void CKeyValues::operator delete(void* pMemory)
{
	// do nothing, because game destructor will automatically free memory
	// KeyValuesSystem->FreeKeyValuesMemory(pMemory);
	(void)pMemory;
}

const char* CKeyValues::GetName()
{
	return KeyValuesSystem->GetStringForSymbol(this->uKeyNameCaseSensitive1 | (this->uKeyNameCaseSensitive2 << 8));
}

CKeyValues* CKeyValues::FromString(const char* szName, const char* szValue)
{
	static auto oFromString = Utilities::Memory::FindPatternV2("client.dll", "55 8B EC 83 E4 F8 81 EC 0C 05"); // @xref: "#empty#", "#int#"
	CKeyValues* pKeyValues = nullptr;

	if (oFromString == 0U)
		return nullptr;

	__asm
	{
		push 0
		mov edx, szValue
		mov ecx, szName
		call oFromString
		add esp, 4
		mov pKeyValues, eax
	}

	return pKeyValues;
}

void CKeyValues::LoadFromBuffer(char const* szResourceName, const char* szBuffer, void* pFileSystem, const char* szPathID, GetSymbolProcFn pfnEvaluateSymbolProc)
{
	using LoadFromBufferFn = void(__thiscall*)(void*, const char*, const char*, void*, const char*, void*, void*);
	static auto oLoadFromBuffer = reinterpret_cast<LoadFromBufferFn>(Utilities::Memory::FindPatternV2("client.dll", ("55 8B EC 83 E4 F8 83 EC 34 53 8B 5D 0C 89"))); // @xref: "KeyValues::LoadFromBuffer(%s%s%s): Begin"
	//assert(oLoadFromBuffer != nullptr);

	oLoadFromBuffer(this, szResourceName, szBuffer, pFileSystem, szPathID, pfnEvaluateSymbolProc, nullptr);
}

bool CKeyValues::LoadFromFile(void* pFileSystem, const char* szResourceName, const char* szPathID, GetSymbolProcFn pfnEvaluateSymbolProc)
{
	//using LoadFromFileFn = bool(__thiscall*)(void*, void*, const char*, const char*, void*);
	//static auto oLoadFromFile = reinterpret_cast<LoadFromFileFn>(Utilities::Memory::FindPatternV2("client.dll", ("55 8B EC 83 E4 F8 83 EC 14 53 56 8B 75 08 57 FF"))); // @xref: "rb"
	//assert(oLoadFromFile != nullptr);

	//return oLoadFromFile(this, pFileSystem, szResourceName, szPathID, pfnEvaluateSymbolProc);
	return false;
}

CKeyValues* CKeyValues::FindKey(const char* szKeyName, const bool bCreate)
{
	using FindKeyFn = CKeyValues * (__thiscall*)(void*, const char*, bool);
	static auto oFindKey = reinterpret_cast<FindKeyFn>(Utilities::Memory::FindPatternV2("client.dll", ("55 8B EC 83 EC 1C 53 8B D9 85 DB")));
	//assert(oFindKey != nullptr);

	return oFindKey(this, szKeyName, bCreate);
}

int CKeyValues::GetInt(const char* szKeyName, const int iDefaultValue)
{
	CKeyValues* pSubKey = this->FindKey(szKeyName, false);

	if (pSubKey == nullptr)
		return iDefaultValue;

	switch (pSubKey->iDataType)
	{
	case TYPE_STRING:
		return std::atoi(pSubKey->szValue);
	case TYPE_WSTRING:
		return _wtoi(pSubKey->wszValue);
	case TYPE_FLOAT:
		return static_cast<int>(pSubKey->flValue);
	case TYPE_UINT64:
		// can't convert, since it would lose data
		//assert(false);
		return 0;
	default:
		break;
	}

	return pSubKey->iValue;
}

float CKeyValues::GetFloat(const char* szKeyName, const float flDefaultValue)
{
	CKeyValues* pSubKey = this->FindKey(szKeyName, false);

	if (pSubKey == nullptr)
		return flDefaultValue;

	switch (pSubKey->iDataType)
	{
	case TYPE_STRING:
		return static_cast<float>(std::atof(pSubKey->szValue));
	case TYPE_WSTRING:
		return std::wcstof(pSubKey->wszValue, nullptr);
	case TYPE_FLOAT:
		return pSubKey->flValue;
	case TYPE_INT:
		return static_cast<float>(pSubKey->iValue);
	case TYPE_UINT64:
		return static_cast<float>(*reinterpret_cast<std::uint64_t*>(pSubKey->szValue));
	case TYPE_PTR:
	default:
		return 0.0f;
	}
}

const char* CKeyValues::GetString(const char* szKeyName, const char* szDefaultValue)
{
	using GetStringFn = const char* (__thiscall*)(void*, const char*, const char*);
	static auto oGetString = reinterpret_cast<GetStringFn>(Utilities::Memory::FindPatternV2("client.dll", ("55 8B EC 83 E4 C0 81 EC ? ? ? ? 53 8B 5D 08")));
	//assert(oGetString != nullptr);

	return oGetString(this, szKeyName, szDefaultValue);
}

void CKeyValues::SetString(const char* szKeyName, const char* szStringValue)
{
	CKeyValues* pSubKey = FindKey(szKeyName, true);

	if (pSubKey == nullptr)
		return;

	using SetStringFn = void(__thiscall*)(void*, const char*);
	static auto oSetString = reinterpret_cast<SetStringFn>(Utilities::Memory::FindPatternV2("client.dll", ("55 8B EC A1 ? ? ? ? 53 56 57 8B F9 8B 08 8B 01")));
	//assert(oSetString != nullptr);

	oSetString(pSubKey, szStringValue);
}

void CKeyValues::SetInt(const char* szKeyName, const int iValue)
{
	CKeyValues* pSubKey = FindKey(szKeyName, true);

	if (pSubKey == nullptr)
		return;

	pSubKey->iValue = iValue;
	pSubKey->iDataType = TYPE_INT;
}

void CKeyValues::SetUint64(const char* szKeyName, const int nLowValue, const int nHighValue)
{
	CKeyValues* pSubKey = FindKey(szKeyName, true);

	if (pSubKey == nullptr)
		return;

	// delete the old value
	delete[] pSubKey->szValue;

	// make sure we're not storing the WSTRING - as we're converting over to STRING
	delete[] pSubKey->wszValue;
	pSubKey->wszValue = nullptr;

	pSubKey->szValue = new char[sizeof(std::uint64_t)];
	*reinterpret_cast<std::uint64_t*>(pSubKey->szValue) = static_cast<std::uint64_t>(nHighValue) << 32ULL | nLowValue;
	pSubKey->iDataType = TYPE_UINT64;
}

IMaterial *CreateMaterial(bool shouldIgnoreZ, bool isLit, bool isWireframe) //credits to ph0ne
{
	static int created = 0;

	static const char tmp[] =
	{
		"\"%s\"\
		\n{\
		\n\t\"$basetexture\" \"vgui/white_additive\"\
		\n\t\"$envmap\" \"\"\
		\n\t\"$model\" \"1\"\
		\n\t\"$flat\" \"1\"\
		\n\t\"$nocull\" \"0\"\
		\n\t\"$selfillum\" \"1\"\
		\n\t\"$halflambert\" \"1\"\
		\n\t\"$nofog\" \"0\"\
		\n\t\"$ignorez\" \"%i\"\
		\n\t\"$znearer\" \"0\"\
		\n\t\"$wireframe\" \"%i\"\
        \n}\n"
	};

	char* baseType = (isLit == true ? "VertexLitGeneric" : "UnlitGeneric");
	char material[512];
	sprintf_s(material, sizeof(material), tmp, baseType, (shouldIgnoreZ) ? 1 : 0, (isWireframe) ? 1 : 0);

	char name[512];
	sprintf_s(name, sizeof(name), "#ayy_meme_%i.vmt", created);
	++created;

	CKeyValues* keyValues = (CKeyValues*)malloc(sizeof(CKeyValues));
	//InitKeyValues(keyValues, baseType);
	keyValues->LoadFromBuffer(name, material);
	//LoadFromBuffer(keyValues, name, material);

	IMaterial *createdMaterial = Interfaces::MaterialSystem->CreateMaterial(name, keyValues);
	createdMaterial->IncrementReferenceCount();

	return createdMaterial;
}

void ForceMaterial(Color color, IMaterial* material, bool useColor, bool forceMaterial)
{
	if (useColor)
	{
		float temp[3] =
		{
			color.r(),
			color.g(),
			color.b()
		};

		temp[0] /= 255.f;
		temp[1] /= 255.f;
		temp[2] /= 255.f;


		float alpha = color.a();

		Interfaces::RenderView->SetBlend(1.0f);
		Interfaces::RenderView->SetColorModulation(temp);
	}

	if (forceMaterial)
		Interfaces::ModelRender->ForcedMaterialOverride(material);
	else
		Interfaces::ModelRender->ForcedMaterialOverride(NULL);

}