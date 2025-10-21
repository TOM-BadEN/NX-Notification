#pragma once

#include <string>
#include <vector>

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
     * @brief 获取指定目录下所有 .ini 文件的完整路径
     * @param dir_path 目录路径
     * @return .ini 文件路径列表（按文件名排序）
     */
    static std::vector<std::string> ListIniFiles(const std::string& dir_path);
    
    /**
     * @brief 批量删除文件
     * @param file_paths 文件路径列表
     * @return true 全部删除成功, false 至少有一个失败
     */
    static bool DeleteFiles(const std::vector<std::string>& file_paths);
    
    /**
     * @brief 读取文件全部内容到内存
     * @param file_path 文件路径
     * @return 文件内容，失败则返回空字符串
     */
    static std::string ReadFileContent(const std::string& file_path);
};

