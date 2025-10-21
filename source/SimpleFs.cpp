#include "SimpleFs.hpp"
#include <dirent.h>
#include <cerrno>
#include <cstdio>

bool SimpleFs::DirectoryExists(const std::string& dir_path) {
    if (dir_path.empty()) {
        return false;
    }
    
    DIR* dir = opendir(dir_path.c_str());
    if (dir) {
        closedir(dir);
        return true;
    }
    
    return false;
}

bool SimpleFs::CreateDirectory(const std::string& dir_path) {
    if (dir_path.empty()) {
        return false;
    }
    
    // 如果目录已存在，返回成功
    if (DirectoryExists(dir_path)) {
        return true;
    }
    
    // 创建目录，权限 0755 (rwxr-xr-x)
    if (mkdir(dir_path.c_str(), 0755) == 0) {
        return true;
    }
    
    // 如果错误是"已存在"，也算成功（处理竞态条件）
    if (errno == EEXIST) {
        return true;
    }
    
    return false;
}

bool SimpleFs::ClearDirectory(const std::string& dir_path) {
    if (dir_path.empty()) {
        return false;
    }
    
    DIR* dir = opendir(dir_path.c_str());
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
            std::string file_path = dir_path;
            if (file_path.back() != '/') {
                file_path += '/';
            }
            file_path += entry->d_name;
            
            if (remove(file_path.c_str()) != 0) {
                success = false;
            }
        }
    }
    
    closedir(dir);
    return success;
}

std::vector<std::string> SimpleFs::ListIniFiles(const std::string& dir_path) {
    std::vector<std::string> result;
    
    if (dir_path.empty()) {
        return result;
    }
    
    DIR* dir = opendir(dir_path.c_str());
    if (!dir) {
        return result;
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
            
            std::string file_path = dir_path;
            if (file_path.back() != '/') {
                file_path += '/';
            }
            file_path += name;
            
            result.push_back(file_path);
        }
    }
    
    closedir(dir);
    return result;
}

bool SimpleFs::DeleteFiles(const std::vector<std::string>& file_paths) {
    if (file_paths.empty()) {
        return true;
    }
    
    bool success = true;
    
    for (const std::string& path : file_paths) {
        if (remove(path.c_str()) != 0) {
            success = false;
        }
    }
    
    return success;
}

std::string SimpleFs::ReadFileContent(const std::string& file_path) {
    if (file_path.empty()) {
        return "";
    }
    
    FILE* file = fopen(file_path.c_str(), "rb");
    if (!file) {
        return "";
    }
    
    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0 || file_size > 4096) {
        fclose(file);
        return "";
    }
    
    // 一次性读取全部内容
    std::string content;
    content.resize(file_size);
    
    size_t read_size = fread(&content[0], 1, file_size, file);
    fclose(file);
    
    if (read_size != (size_t)file_size) {
        return "";
    }
    
    return content;
}

