#pragma once

namespace plat 
{
template<typename T>
struct FPtr
{
    size_t len;
    T* data;
};
typedef FPtr<char> FilePath;
typedef FPtr<void> FileContents;

bool show_file_prompt(FilePath* path);
void unload_path(FilePath path);

FileContents load_entire_file(FilePath path);
void unload_file(FileContents contents);

void update_input();
};
