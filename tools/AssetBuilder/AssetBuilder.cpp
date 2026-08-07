#include <windows.h>

#include "Types.h"
#include "Memory.h"
#include "Strings.h"
#include "Win32FileIO.h"
#include "EngaFormat.h"
#include "Loaders/GLTF.h"
#include "Loaders/TGA.h"
#include "Loaders/HDR.h"
#include "Loaders/Cubemap.h"

#define MAX_PACK_ASSETS 64

struct asset_table
{
    asset_descriptor Entries[MAX_PACK_ASSETS];
    void       *Data[MAX_PACK_ASSETS];
    uint32      Count;
};

struct image_asset
{
    const char        *Name;
    void              *Pixels;
    asset_image_format Format;
    bool32             IsSRGB;
    bool32             IsAtlas;
    uint32             Width;
    uint32             Height;
    uint32             Layers;
};

internal asset_descriptor *AddAsset(asset_table *Table, asset_type Type, const char *Name, void *Data, uint64 Size)
{
    Assert(Table->Count < MAX_PACK_ASSETS);

    asset_descriptor *Entry = &Table->Entries[Table->Count];
    *Entry = {};
    Entry->Type = (uint32)Type;
    AppendString(Entry->Name, ENGA_MAX_ASSET_NAME, 0, Name);
    Entry->Size = Size;

    Table->Data[Table->Count] = Data;
    Table->Count++;

    return Entry;
}

internal void AddMesh(asset_table *Table, const char *Name, void *Blob, uint64 BlobSize, uint32 VertexCount, uint32 IndexCount)
{
    asset_descriptor *Desc = AddAsset(Table, Asset_Mesh, Name, Blob, BlobSize);
    Desc->Mesh.VertexCount = VertexCount;
    Desc->Mesh.IndexCount  = IndexCount;
}

internal void AddImage(asset_table *Table, image_asset Image)
{
    Assert(Image.Name);
    Assert(Image.Pixels);
    Assert(Image.Width && Image.Height && Image.Layers);

    uint64 ByteSize = (uint64)Image.Width * Image.Height * Image.Layers * AssetImageFormatBytes(Image.Format);

    asset_descriptor *Desc = AddAsset(Table, Asset_Image, Image.Name, Image.Pixels, ByteSize);
    Desc->Image.Format  = (uint32)Image.Format;
    Desc->Image.IsSRGB  = Image.IsSRGB;
    Desc->Image.IsAtlas = Image.IsAtlas;
    Desc->Image.Width   = Image.Width;
    Desc->Image.Height  = Image.Height;
    Desc->Image.Layers  = Image.Layers;
}

internal void AddSkyCubemap(asset_table *Table, memory_arena *Arena, const char *Name, const char *Path, uint32 FaceSize)
{
    file_contents File = Win32ReadEntireFile(Path);
    Assert(File.Data);

    loaded_hdr Equirect = ParseHDR(Arena, File.Data, File.Size);
    Assert(Equirect.Pixels);

    loaded_cubemap Cube = EquirectToCubemap(Arena, &Equirect, FaceSize);
    Assert(Cube.Pixels);

    image_asset Cubemap = {};
    Cubemap.Name   = Name;
    Cubemap.Pixels = Cube.Pixels;
    Cubemap.Format = ImageFormat_RGBA16F;
    Cubemap.Width  = Cube.FaceSize;
    Cubemap.Height = Cube.FaceSize;
    Cubemap.Layers = 6;
    AddImage(Table, Cubemap);

    DebugLog("AssetBuilder: '%s' %ux%u equirect -> %u^2 x6 cubemap RGBA16F (%llu bytes)\n",
             Name, Equirect.Width, Equirect.Height, Cube.FaceSize, Cube.ByteSize);
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

    return Win32ReadEntireFile(Path);
}

internal void AddGLTF(asset_table *Table, memory_arena *Arena, const char *Path)
{
    file_contents File = Win32ReadEntireFile(Path);
    Assert(File.Data);

    gltf_file Gltf = ParseGLTF(Arena, File.Data, File.Size);
    Assert(Gltf.Root);

    if (!Gltf.Bin)
    {
        char *Uri = JsonCString(JsonGet(JsonAt(JsonGet(Gltf.Root, "buffers"), 0), "uri"));
        Assert(Uri);

        file_contents Bin = ReadSiblingFile(Path, Uri);
        Assert(Bin.Data);

        Gltf.Bin     = (uint8 *)Bin.Data;
        Gltf.BinSize = Bin.Size;
    }

    json_value *Meshes = JsonGet(Gltf.Root, "meshes");
    for (json_member *Member = Meshes ? Meshes->First : 0; Member; Member = Member->Next)
    {
        json_value *Mesh = Member->Value;

        char *Name = JsonCString(JsonGet(Mesh, "name"));
        Assert(Name);

        gltf_geometry Geometry = GLTFMeshGeometry(Arena, &Gltf, Mesh);
        Assert(Geometry.Blob);

        AddMesh(Table, Name, Geometry.Blob, Geometry.BlobSize, Geometry.VertexCount, Geometry.IndexCount);
    }

    json_value *Images = JsonGet(Gltf.Root, "images");
    for (json_member *Member = Images ? Images->First : 0; Member; Member = Member->Next)
    {
        json_value *Image = Member->Value;

        char *Name = JsonCString(JsonGet(Image, "name"));
        char *Uri  = JsonCString(JsonGet(Image, "uri"));
        Assert(Name);
        Assert(Uri);

        file_contents ImageFile = ReadSiblingFile(Path, Uri);
        Assert(ImageFile.Data);

        loaded_bitmap Bitmap = ParseTGA(Arena, ImageFile.Data, ImageFile.Size);
        Assert(Bitmap.Pixels);

        image_asset Texture = {};
        Texture.Name   = Name;
        Texture.Pixels = Bitmap.Pixels;
        Texture.Format = ImageFormat_RGBA8;
        Texture.IsSRGB = true;
        Texture.Width  = Bitmap.Width;
        Texture.Height = Bitmap.Height;
        Texture.Layers = 1;
        AddImage(Table, Texture);
    }
}

internal void WriteTable(asset_table *Table, const char *Path)
{
    asset_file_header Header = {};
    Header.Magic            = ENGA_MAGIC;
    Header.Version          = ENGA_VERSION;
    Header.AssetCount       = Table->Count;
    Header.AssetTableOffset = (uint32)sizeof(asset_file_header);

    uint64 DataOffset = sizeof(asset_file_header) + (uint64)Table->Count * sizeof(asset_descriptor);
    for (uint32 i = 0; i < Table->Count; ++i)
    {
        Table->Entries[i].Offset = DataOffset;
        DataOffset += Table->Entries[i].Size;
    }

    HANDLE File = CreateFileA(Path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    Assert(File != INVALID_HANDLE_VALUE);

    DWORD Written = 0;

    BOOL Ok = WriteFile(File, &Header, (DWORD)sizeof(Header), &Written, 0);
    Assert(Ok);

    Ok = WriteFile(File, Table->Entries, (DWORD)(Table->Count * sizeof(asset_descriptor)), &Written, 0);
    Assert(Ok);

    for (uint32 i = 0; i < Table->Count; ++i)
    {
        Ok = WriteFile(File, Table->Data[i], (DWORD)Table->Entries[i].Size, &Written, 0);
        Assert(Ok);
    }

    CloseHandle(File);
}

int main(int ArgCount, char **Args)
{
    const char *OutPath = (ArgCount > 1) ? Args[1] : "assets.enga";

    uint32 ArenaSize = (uint32)Megabytes(64);
    void  *ArenaMemory = VirtualAlloc(0, ArenaSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Assert(ArenaMemory);

    memory_arena Arena;
    InitializeArena(&Arena, ArenaSize, ArenaMemory);

    asset_table Table = {};

    AddGLTF(&Table, &Arena, "..\\assets\\models\\TestShapes.gltf");
    AddSkyCubemap(&Table, &Arena, "sky", "..\\assets\\images\\sky.hdr", 512);
    WriteTable(&Table, OutPath);

    DebugLog("AssetBuilder: '%s' written (%u assets)\n", OutPath, Table.Count);

    return 0;
}
