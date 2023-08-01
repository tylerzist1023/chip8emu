#pragma once

namespace plat 
{
struct FileContents
{
	size_t size;
	void* memory;
};
struct FilePath
{
	size_t len;
	char* str;
};

bool show_file_prompt(FilePath* path);
void unload_path(FilePath path);

FileContents load_entire_file(FilePath path);
void unload_file(FileContents contents);

void update_input();
};
