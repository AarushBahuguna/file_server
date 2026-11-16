#include <stdio.h>

int mock_last_block = -1;
int mock_last_op = -1;
int mock_read_count = 0;
int mock_write_count = 0;

int disk_raw_read(int block)
{
    mock_last_block = block;
    mock_last_op = 0;
    mock_read_count++;
    return 0;
}

int disk_raw_write(int block)
{
    mock_last_block = block;
    mock_last_op = 1;
    mock_write_count++;
    return 0;
}
