#include "lfs.h"
#include "lfs_util.h"
#include "filesystem_port.h"
#include "storage_api.h"
#include "syscfg_id.h"
#include "errno.h"

#include "app_log.h"

int syscfg_init(void)
{
    fs_status stat;
    int ret = fs_stat(PATH_CAT("syscfg"), &stat);
    
    if (ret == 0 && stat.type == FS_DIR_ENTRY_DIR) {
        return 0;
    }
    
    // 目录不存在，尝试创建
    if (ret != 0) {
        ret = fs_mkdir(PATH_CAT("syscfg"));
        if (ret != 0 && ret != -EEXIST) {
            app_log_error("Failed to create directory %s: %d\r\n", PATH_CAT("syscfg"), ret);
            return ret;
        }
    }
    return 0;
}

int syscfg_read(uint16_t item_id, void *buf, uint16_t len)
{
    char filepath[MAX_PATH_LEN];
    snprintf(filepath, sizeof(filepath), PATH_CAT("syscfg/item_%d.cfg"), item_id);
    
    fs_file_t config_file;
    int ret = fs_open(&config_file, filepath, FS_O_READ);
    
    // 如果文件不存在，则创建文件
    if (ret == -ENOENT) {
        ret = fs_open(&config_file, filepath, FS_O_CREATE | FS_O_RDWR);
        if (ret != 0) {
            app_log_error("Failed to create config file for item %d: %d\r\n", item_id, ret);
            return ret;
        }
        
        // 新创建的文件没有数据，返回0表示成功但无数据
        fs_close(&config_file);
        memset(buf, 0x00, len);
        return 0;
    }

    // 处理其他类型的打开错误
    if (ret != 0) {
        app_log_error("Failed to open config file for item %d: %d\r\n", item_id, ret);
        return ret;
    }
    
    // 读取文件内容
    ssize_t bytes_read = fs_read(&config_file, buf, len);
    if (bytes_read < 0) {
        app_log_error("Read failed for item %d: %d\r\n", item_id, (int)bytes_read);
        fs_close(&config_file);
        return bytes_read;
    }
    
    fs_close(&config_file);
    return bytes_read;
}

int syscfg_write(uint16_t item_id, void *buf, uint16_t len)
{
    char filepath[MAX_PATH_LEN];
    snprintf(filepath, sizeof(filepath), PATH_CAT("syscfg/item_%d.cfg"), item_id);
    
    fs_file_t config_file;
    int ret = fs_open(&config_file, filepath, FS_O_CREATE | FS_O_WRITE);
    if (ret != 0) {
        app_log_error("Failed to create/write config for item %d: %d\r\n", item_id, ret);
        return ret;
    }
    
    // 写入配置数据
    ssize_t bytes_written = fs_write_ts(&config_file, buf, len);
    if (bytes_written < 0) {
        app_log_error("Write failed for item %d: %d\r\n", item_id, (int)bytes_written);
        fs_close(&config_file);
        return bytes_written;
    }
    
    fs_sync(&config_file);
    fs_close(&config_file);
    return bytes_written;
}

