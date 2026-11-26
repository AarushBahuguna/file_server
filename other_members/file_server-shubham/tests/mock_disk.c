#include <stdio.h>
#include <string.h>

int mock_last_block = -1;
int mock_last_op = -1;
int mock_read_count = 0;
int mock_write_count = 0;
static int mock_block_reads[200];
static int mock_block_writes[200];

void mock_reset_blocks(void)
{
    memset(mock_block_reads, 0, sizeof(mock_block_reads));
    memset(mock_block_writes, 0, sizeof(mock_block_writes));
}

int mock_block_read_count(int block)
{
    return (block >= 0 && block < 200) ? mock_block_reads[block] : 0;
}

int mock_block_write_count(int block)
{
    return (block >= 0 && block < 200) ? mock_block_writes[block] : 0;
}

int disk_raw_read(int block)
{
    mock_last_block = block;
    mock_last_op = 0;
    mock_read_count++;
    if (block >= 0 && block < 200) {
        mock_block_reads[block]++;
    }
    return 0;
}

int disk_raw_write(int block)
{
    mock_last_block = block;
    mock_last_op = 1;
    mock_write_count++;
    if (block >= 0 && block < 200) {
        mock_block_writes[block]++;
    }
    return 0;
}
