#include "SimpleFs.hpp"
#include <dirent.h>
#include <cerrno>
#include <cstdio>

// 定义静态成员
char SimpleFs::s_PathBuffer[256];     // 路径缓冲区
char SimpleFs::s_ContentBuffer[256];  // 文件内容缓冲区

bool SimpleFs::DirectoryExists(const char* dir_path) {
    if (!dir_path || dir_path[0] == '\0') {
        return false;
    }
    
    DIR* dir = opendir(dir_path);
    if (dir) {
        closedir(dir);
        return true;
    }
    
    return false;
}

bool SimpleFs::CreateDirectory(const char* dir_path) {
    if (!dir_path || dir_path[0] == '\0') {
        return false;
    }
    
    // 如果目录已存在，返回成功
    if (DirectoryExists(dir_path)) {
        return true;
    }
    
    // 创建目录，权限 0755 (rwxr-xr-x)
    if (mkdir(dir_path, 0755) == 0) {
        return true;
    }
    
    // 如果错误是"已存在"，也算成功（处理竞态条件）
    if (errno == EEXIST) {
        return true;
    }
    
    return false;
}

bool SimpleFs::ClearDirectory(const char* dir_path) {
    if (!dir_path || dir_path[0] == '\0') {
        return false;
    }
    
    DIR* dir = opendir(dir_path);
    if (!dir) {
        return false;
    }
    
    struct dirent* entry;
    bool success = true;
    
    while ((entry = readdir(dir)) != nullptr) {
        // 跳过 "." 和 ".."
        if (entry->d_name[0] == '.' && 
            (entry->d_name[1] == '\0' || 
             (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }
        
        // 只删除普通文件，不删除子目录
        if (entry->d_type == DT_REG) {
            char file_path[1024];
            snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry->d_name);
            
            if (remove(file_path) != 0) {
                success = false;
            }
        }
    }
    
    closedir(dir);
    return success;
}

const char* SimpleFs::GetFirstIniFile(const char* dir_path) {
    if (!dir_path || dir_path[0] == '\0') {
        return nullptr;
    }
    
    DIR* dir = opendir(dir_path);
    if (!dir) {
        return nullptr;
    }
    
    struct dirent* entry;
    
    while ((entry = readdir(dir)) != nullptr) {
        // 只处理普通文件
        if (entry->d_type != DT_REG) {
            continue;
        }
        
        // 检查是否以 .ini 结尾
        const char* name = entry->d_name;
        size_t len = 0;
        while (name[len] != '\0') len++;
        
        if (len > 4 && 
            name[len-4] == '.' &&
            (name[len-3] == 'i' || name[len-3] == 'I') &&
            (name[len-2] == 'n' || name[len-2] == 'N') &&
            (name[len-1] == 'i' || name[len-1] == 'I')) {
            
            // 这句话忽略编译器警告
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wformat-truncation"
            // 拼接到路径缓冲区
            snprintf(s_PathBuffer, sizeof(s_PathBuffer), "%s/%s", dir_path, name);
            #pragma GCC diagnostic pop
            closedir(dir);
            return s_PathBuffer;  // 返回路径缓冲区指针
        }
    }
    
    closedir(dir);
    return nullptr;  // 没找到
}

bool SimpleFs::DeleteFile(const char* file_path) {
    if (!file_path || file_path[0] == '\0') {
        return false;
    }
    
    return remove(file_path) == 0;
}

const char* SimpleFs::ReadFileContent(const char* file_path) {
    if (!file_path || file_path[0] == '\0') {
        return nullptr;
    }
    
    FILE* file = fopen(file_path, "rb");
    if (!file) {
        return nullptr;
    }
    
    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // 检查大小（文件太大或为空）
    if (file_size <= 0 || file_size >= (long)sizeof(s_ContentBuffer)) {
        fclose(file);
        return nullptr;
    }
    
    // 读取到内容缓冲区
    size_t read_size = fread(s_ContentBuffer, 1, file_size, file);
    s_ContentBuffer[read_size] = '\0';  // 添加 null terminator
    fclose(file);
    
    if (read_size != (size_t)file_size) {
        return nullptr;
    }
    
    return s_ContentBuffer;  // 返回内容缓冲区指针
}

