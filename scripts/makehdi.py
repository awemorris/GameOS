import os
import struct

def convert_raw_to_hdi_correct(raw_path, hdi_path):
    # PC-98 HDDの標準的なジオメトリ
    sector_size = 512
    heads = 8
    sectors_per_track = 17
    
    raw_size = os.path.getsize(raw_path)
    
    # 1シリンダあたりのバイト数 (8 * 17 * 512 = 69,632 バイト)
    bytes_per_cylinder = heads * sectors_per_track * sector_size
    
    # 端数が出ないようにシリンダ数を切り上げ計算
    cylinders = (raw_size + bytes_per_cylinder - 1) // bytes_per_cylinder
    
    cylinders = 615

    # ジオメトリに完全に一致するディスクの純粋なデータサイズ
    disk_data_size = cylinders * bytes_per_cylinder
    
    print(f"✨ ジオメトリ情報 ✨")
    print(f"Cylinders: {cylinders}, Heads: {heads}, Sectors/Track: {sectors_per_track}")
    print(f"Sector Size: {sector_size} bytes")
    print(f"Disk Data Size: {disk_data_size} bytes")
    
    # 4096バイトのヘッダをゼロ埋めで作成
    header = bytearray(4096)
    
    # 【ここが修正ポイント！】
    # 正しいAnex86 HDIヘッダのオフセット構造 (すべて32bitリトルエンディアン)
    struct.pack_into('<I', header, 0, 0)                  # 0x00: Dummy (0)
    struct.pack_into('<I', header, 4, 0)                  # 0x04: Type (0)
    struct.pack_into('<I', header, 8, 4096)               # 0x08: ヘッダサイズ (4096固定)
    struct.pack_into('<I', header, 12, disk_data_size)    # 0x0C: HDD Size (データ部のバイト数)
    struct.pack_into('<I', header, 16, sector_size)       # 0x10: Sector Size (512)
    struct.pack_into('<I', header, 20, sectors_per_track) # 0x14: Sectors (17)
    struct.pack_into('<I', header, 24, heads)             # 0x18: Surfaces/Heads (8)
    struct.pack_into('<I', header, 28, cylinders)         # 0x1C: Cylinders
    
    with open(hdi_path, 'wb') as f_out:
        # 1. 正しいヘッダを書き込む
        f_out.write(header)
        
        # 2. 元のRAWデータを書き込む
        with open(raw_path, 'rb') as f_in:
            raw_data = f_in.read()
            f_out.write(raw_data)
            
        # 3. サイズがジオメトリと完全に一致するように0埋め (パディング)
        padding_size = disk_data_size - len(raw_data)
        if padding_size > 0:
            f_out.write(b'\x00' * padding_size)
            print(f"パディング: {padding_size} bytes 追加してサイズを完璧に合わせたよ！")
            
    print(f"🎉 変換完了！ {hdi_path} を出力したよ！")

# 実行してみてね！
convert_raw_to_hdi_correct("bootdisk.raw", "bootdisk.hdi")
