#pragma once

class SimpleFs {
public:
    /**
     * @brief 检查目录是否存在
     * @param dir_path 目录路径
     * @return true 存在, false 不存在
     */
    static bool DirectoryExists(const char* dir_path);
    
    /**
     * @brief 创建目录
     * @param dir_path 目录路径
     * @return true 创建成功或已存在, false 创建失败
     */
    static bool CreateDirectory(const char* dir_path);
    
    /**
     * @brief 清空目录下所有文件（不删除子目录）
     * @param dir_path 目录路径
     * @return true 成功, false 失败
     */
    static bool ClearDirectory(const char* dir_path);
    
    /**
     * @brief 获取指定目录下第一个 .ini 文件的完整路径
     * @param dir_path 目录路径
     * @return 找到的文件路径指针，未找到返回 nullptr
     * @note 返回的指针指向内部静态缓冲区，下次调用会覆盖
     */
    static const char* GetFirstIniFile(const char* dir_path);
    
    /**
     * @brief 删除单个文件
     * @param file_path 文件路径
     * @return true 删除成功, false 删除失败
     */
    static bool DeleteFile(const char* file_path);
    
    /**
     * @brief 读取文件全部内容到内存
     * @param file_path 文件路径
     * @return 文件内容指针，失败返回 nullptr
     * @note 返回的指针指向内部静态缓冲区，下次调用会覆盖
     */
    static const char* ReadFileContent(const char* file_path);
    
private:
    static char s_PathBuffer[256];     // 路径缓冲区
    static char s_ContentBuffer[256];  // 文件内容缓冲区
};

