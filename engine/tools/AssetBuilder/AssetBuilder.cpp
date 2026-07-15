#include <windows.h>

#include "Types.h"
#include "Memory.h"
#include "Strings.h"
#include "KBNFormat.h"
#include "Loaders/GLTF.h"
#include "Loaders/TGA.h"

struct file_contents
{
    void  *Data;
    uint32 Size;
};

internal file_contents ReadEntireFile(const char *Path)
{
    file_contents Result = {};

    HANDLE File = CreateFileA(Path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (File == INVALID_HANDLE_VALUE)
    {
        return Result;
    }

    LARGE_INTEGER FileSize;
    if (GetFileSizeEx(File, &FileSize))
    {
        uint32 Size32 = SafeTruncateUInt64((uint64)FileSize.QuadPart);
        Result.Data = VirtualAlloc(0, Size32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        DWORD BytesRead = 0;
        if (Result.Data && ReadFile(File, Result.Data, Size32, &BytesRead, 0) && BytesRead == Size32)
        {
            Result.Size = Size32;
        }
        else
        {
            Result.Data = 0;
        }
    }

    CloseHandle(File);
    return Result;
}

#define MAX_PACK_ASSETS 64

struct pack_state
{
    asset_entry Entries[MAX_PACK_ASSETS];
    void       *Data[MAX_PACK_ASSETS];
    uint32      Count;
};

internal asset_entry *AddAsset(pack_state *Pack, asset_type Type, const char *Name, void *Data, uint64 Size)
{
    Assert(Pack->Count < MAX_PACK_ASSETS);

    asset_entry *Entry = &Pack->Entries[Pack->Count];
    *Entry = {};
    Entry->Type = (uint32)Type;
    AppendString(Entry->Name, KBN_MAX_ASSET_NAME, 0, Name);
    Entry->Size = Size;

    Pack->Data[Pack->Count] = Data;
    Pack->Count++;

    return Entry;
}

internal void AddMesh(pack_state *Pack, const char *Name, real32 *Vertices, uint32 VertexCount)
{
    asset_entry *Entry = AddAsset(Pack, Asset_Mesh, Name, Vertices, (uint64)VertexCount * KBN_VERTEX_FLOATS * sizeof(real32));
    Entry->Mesh.VertexCount = VertexCount;
}

internal void AddImagePixels(pack_state *Pack, const char *Name, void *Pixels, uint32 Width, uint32 Height, uint32 SRGB)
{
    asset_entry *Entry = AddAsset(Pack, Asset_Image, Name, Pixels, (uint64)Width * Height * 4);
    Entry->Image.Width  = Width;
    Entry->Image.Height = Height;
    Entry->Image.SRGB   = SRGB;
}

internal file_contents ReadSiblingFile(const char *BasePath, const char *Uri)
{
    char Path[512];
    uint32 DirLength = 0;
    for (uint32 i = 0; BasePath[i]; ++i)
    {
        if (BasePath[i] == '\\' || BasePath[i] == '/')
        {
            DirLength = i + 1;
        }
    }

    uint32 At = 0;
    for (; At < DirLength && At < (uint32)ArrayCount(Path) - 1; ++At)
    {
        Path[At] = BasePath[At];
    }
    Path[At] = 0;
    AppendString(Path, (uint32)ArrayCount(Path), At, Uri);

    return ReadEntireFile(Path);
}

internal bool32 AddGLTF(pack_state *Pack, memory_arena *Arena, const char *Path)
{
    file_contents File = ReadEntireFile(Path);
    if (!File.Data)
    {
        DebugLog("AssetBuilder: cannot open '%s'\n", Path);
        return false;
    }

    gltf_file Gltf = ParseGLTF(Arena, File.Data, File.Size);
    if (!Gltf.Root)
    {
        DebugLog("AssetBuilder: cannot parse '%s'\n", Path);
        return false;
    }

    if (!Gltf.Bin)
    {
        char *Uri = JsonCString(JsonGet(JsonAt(JsonGet(Gltf.Root, "buffers"), 0), "uri"));
        if (Uri)
        {
            file_contents Bin = ReadSiblingFile(Path, Uri);
            Gltf.Bin     = (uint8 *)Bin.Data;
            Gltf.BinSize = Bin.Size;
        }
    }

    if (!Gltf.Bin)
    {
        DebugLog("AssetBuilder: '%s' has no binary buffer\n", Path);
        return false;
    }

    json_value *Meshes = JsonGet(Gltf.Root, "meshes");
    for (json_member *Member = Meshes ? Meshes->First : 0; Member; Member = Member->Next)
    {
        json_value *Mesh = Member->Value;
        char *Name = JsonCString(JsonGet(Mesh, "name"));

        uint32  VertexCount = 0;
        real32 *Vertices = GLTFMeshVertices(Arena, &Gltf, Mesh, &VertexCount);
        if (!Name || !Vertices)
        {
            DebugLog("AssetBuilder: mesh '%s' in '%s' skipped\n", Name ? Name : "?", Path);
            continue;
        }

        AddMesh(Pack, Name, Vertices, VertexCount);
    }

    json_value *Images = JsonGet(Gltf.Root, "images");
    for (json_member *Member = Images ? Images->First : 0; Member; Member = Member->Next)
    {
        json_value *Image = Member->Value;
        char *Name = JsonCString(JsonGet(Image, "name"));
        char *Uri  = JsonCString(JsonGet(Image, "uri"));

        loaded_bitmap Bitmap = {};
        if (Uri)
        {
            file_contents ImageFile = ReadSiblingFile(Path, Uri);
            if (ImageFile.Data)
            {
                Bitmap = ParseTGA(Arena, ImageFile.Data, ImageFile.Size);
            }
        }

        if (!Name || !Bitmap.Pixels)
        {
            DebugLog("AssetBuilder: image '%s' in '%s' skipped\n", Name ? Name : "?", Path);
            continue;
        }

        AddImagePixels(Pack, Name, Bitmap.Pixels, Bitmap.Width, Bitmap.Height, 1);
    }

    return true;
}

internal bool32 WritePack(pack_state *Pack, const char *Path)
{
    asset_file_header Header = {};
    Header.Magic            = KBN_MAGIC;
    Header.Version          = KBN_VERSION;
    Header.AssetCount       = Pack->Count;
    Header.AssetTableOffset = (uint32)sizeof(asset_file_header);

    uint64 DataOffset = sizeof(asset_file_header) + (uint64)Pack->Count * sizeof(asset_entry);
    for (uint32 i = 0; i < Pack->Count; ++i)
    {
        Pack->Entries[i].Offset = DataOffset;
        DataOffset += Pack->Entries[i].Size;
    }

    HANDLE File = CreateFileA(Path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (File == INVALID_HANDLE_VALUE)
    {
        DebugLog("AssetBuilder: cannot create '%s'\n", Path);
        return false;
    }

    bool32 Ok = true;
    DWORD Written = 0;
    Ok = Ok && WriteFile(File, &Header, (DWORD)sizeof(Header), &Written, 0);
    Ok = Ok && WriteFile(File, Pack->Entries, (DWORD)(Pack->Count * sizeof(asset_entry)), &Written, 0);
    for (uint32 i = 0; Ok && i < Pack->Count; ++i)
    {
        Ok = Ok && WriteFile(File, Pack->Data[i], (DWORD)Pack->Entries[i].Size, &Written, 0);
    }

    CloseHandle(File);
    return Ok;
}

int main(int ArgCount, char **Args)
{
    const char *OutPath = (ArgCount > 1) ? Args[1] : "assets.kbn";

    uint32 ArenaSize = (uint32)Megabytes(64);
    memory_arena Arena;
    InitializeArena(&Arena, ArenaSize, VirtualAlloc(0, ArenaSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));

    pack_state Pack = {};

    if (!AddGLTF(&Pack, &Arena, "..\\engine\\assets\\models\\TestShapes.gltf"))
    {
        return 1;
    }

    if (!WritePack(&Pack, OutPath))
    {
        return 1;
    }

    DebugLog("AssetBuilder: '%s' written (%u assets)\n", OutPath, Pack.Count);
    return 0;
}
