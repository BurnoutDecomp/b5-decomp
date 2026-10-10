#pragma once

// FLAG PC-platform leaf: resource entries own native D3D textures separately
// from the console-style heap bytes. Entry addresses remain stable during
// header/pixel relocation. An unfixed serialized header never enters this table.
namespace renderengine
{
    void TextureResource_OnEntryFixedUp(const void* lpOwner, void* lpHeader);
    void TextureResource_OnEntryFreed(const void* lpOwner, void* lpHeader);
}
