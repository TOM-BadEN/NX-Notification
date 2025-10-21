#pragma once

#include <string>

class SimpleFs {
public:
    /**
     * @brief 检查目录是否存在
     * @param dir_path 目录路径
     * @return true 存在, false 不存在
     */
    static bool DirectoryExists(const std::string& dir_path);
    
    /**
     * @brief 创建目录
     * @param dir_path 目录路径
     * @return true 创建成功或已存在, false 创建失败
     */
    static bool CreateDirectory(const std::string& dir_path);
    
    /**
     * @brief 清空目录下所有文件（不删除子目录）
     * @param dir_path 目录路径
     * @return true 成功, false 失败
     */
    static bool ClearDirectory(const std::string& dir_path);
    
    /**
     * @brief 获取指定目录下第一个 .ini 文件的完整路径
     * @param dir_path 目录路径
     * @return .ini 文件路径，如果没有则返回空字符串
     */
    static std::string GetFirstIniFile(const std::string& dir_path);
    
    /**
     * @brief 删除单个文件
     * @param file_path 文件路径
     * @return true 删除成功, false 删除失败
     */
    static bool DeleteFile(const std::string& file_path);
    
    /**
     * @brief 读取文件全部内容到内存
     * @param file_path 文件路径
     * @return 文件内容，失败则返回空字符串
     */
    static std::string ReadFileContent(const std::string& file_path);
};

