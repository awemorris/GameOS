#include <stdint.h>

// I/Oポートの基本定義（PC/AT互換機の標準）
#define IDE_PRIMARY_BASE   0x1F0
#define IDE_SECONDARY_BASE 0x170

// ベースポートからのオフセット
#define IDE_REG_DATA       0
#define IDE_REG_ERR        1
#define IDE_REG_FEATURES   1
#define IDE_REG_SECCOUNT   2
#define IDE_REG_LBA_LOW    3
#define IDE_REG_LBA_MID    4
#define IDE_REG_LBA_HIGH   5
#define IDE_REG_DRV_HEAD   6
#define IDE_REG_STATUS     7
#define IDE_REG_COMMAND    7

// IDEコマンド
#define IDE_CMD_READ_RETRY 0x20 // 割り込みあり読み込みだけど、ポーリングでも使えるよ

// ステータスレジスタのビット
#define IDE_STATUS_BSY     0x80 // Busy
#define IDE_STATUS_DRDY    0x40 // Drive Ready
#define IDE_STATUS_DF      0x20 // Drive Fault
#define IDE_STATUS_DRQ     0x08 // Data Request
#define IDE_STATUS_ERR     0x01 // Error

// インラインアセンブラ用のI/Oラッパー（環境に合わせて調整してね）
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/**
 * IDE HDD 読み取りルーチン (LBA 28ビット / PIOモード)
 * * @param is_secondary 0: プライマリ, 1: セカンダリ
 * @param is_slave     0: マスター, 1: スレーブ
 * @param lba          リニアセクタ番号 (LBA28なので最大2^28-1まで)
 * @param sector_count 読み取るセクタ数 (0を指定すると256セクタ扱いになるよ)
 * @param buffer       データを格納するバッファポインタ (1セクタあたり512バイト必要)
 * @return int         0: 成功, -1: エラー
 */
int ide_read_sectors(int is_secondary, int is_slave, uint32_t lba, uint8_t sector_count, uint16_t *buffer) {
    // 1. チャンネル（プライマリ/セカンダリ）に応じたベースポートの決定
    uint16_t io_base = is_secondary ? IDE_SECONDARY_BASE : IDE_PRIMARY_BASE;

    // 2. ドライブ/ヘッドレジスタに書き込む値の用意 (LBAモードフラグ + マスター/スレーブ + LBAの最上位4ビット)
    // 0xE0 は LBAモード有効(ビット6) と 昔の名残のビット7,5 を1にするためだよ
    uint8_t drive_select = 0xE0;
    if (is_slave) {
        drive_select |= 0x10; // ビット4を1にするとスレーブ選択
    }
    drive_select |= ((lba >> 24) & 0x0F); // LBAの24〜27ビット目を割り当て

    // 3. ドライブがビジー（BSY）じゃなくなるまでちょっと待つ（簡易的な処理）
    while (inb(io_base + IDE_REG_STATUS) & IDE_STATUS_BSY);

    // 4. 各レジスタにパラメータをセットしていくよ
    outb(io_base + IDE_REG_DRV_HEAD, drive_select);
    outb(io_base + IDE_REG_SECCOUNT, sector_count);
    outb(io_base + IDE_REG_LBA_LOW,  (uint8_t)(lba & 0xFF));
    outb(io_base + IDE_REG_LBA_MID,  (uint8_t)((lba >> 8) & 0xFF));
    outb(io_base + IDE_REG_LBA_HIGH, (uint8_t)((lba >> 16) & 0xFF));

    // 5. 読み込みコマンド（0x20）を発行！
    outb(io_base + IDE_REG_COMMAND, IDE_CMD_READ_RETRY);

    // 6. セクタ数分、ループを回してデータを読み取るよ
    // ※ sector_countが0のときは256セクタとして扱うのがIDEの仕様だよ
    int real_count = (sector_count == 0) ? 256 : sector_count;

    for (int s = 0; s < real_count; s++) {
        // 7. ポーリング開始！BSYが消えて、DRQ（データ準備完了）が1になるのを待つ
        while (1) {
            uint8_t status = inb(io_base + IDE_REG_STATUS);
            if ((status & IDE_STATUS_BSY) == 0) {
                if (status & IDE_STATUS_ERR) {
                    return -1; // 何かエラーが起きたらバイバイ
                }
                if (status & IDE_STATUS_DRQ) {
                    break; // 準備OK！ループを抜けて読み込みへ
                }
            }
        }

        // 8. 1セクタ分（256ワード ＝ 512バイト）をデータポートから読み込む
        for (int i = 0; i < 256; i++) {
            *buffer = inw(io_base + IDE_REG_DATA);
            buffer++;
        }
    }

    return 0; // 無事に全部読めたら成功！
}
