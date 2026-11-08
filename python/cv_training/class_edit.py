from pathlib import Path

def change_classes_to_one(root_dir):
    root_path = Path(root_dir)
    
    if not root_path.exists():
        print(f"Error: The path '{root_path}' does not exist.")
        return

    updated_files_count = 0
    print(f"Scanning '{root_path}' and all subfolders...")
    
    # rglob('*.txt') recursively finds all text files
    for txt_file in root_path.rglob("*.txt"):
        try:
            content = txt_file.read_text(encoding="utf-8")
            lines = content.splitlines()
            
            new_lines = []
            file_modified = False
            
            for line in lines:
                parts = line.strip().split()
                if parts:
                    # Change the first value (class ID) to "1" if it isn't already
                    if parts[0] != "0":
                        parts[0] = "0"
                        file_modified = True
                    new_lines.append(" ".join(parts))
                else:
                    new_lines.append(line)
            
            # Save the file only if changes were made
            if file_modified:
                txt_file.write_text("\n".join(new_lines) + ("\n" if lines else ""), encoding="utf-8")
                print(f"Updated: {txt_file}")
                updated_files_count += 1
                
        except Exception as e:
            print(f"Error processing {txt_file}: {e}")
            
    print(f"\nDone! Total text files updated: {updated_files_count}")

if __name__ == "__main__":
    # --- PUT YOUR FOLDER PATH HERE ---
    # Examples:
    # target_directory = r"C:\Users\YourName\Datasets\images"  (Windows raw string)
    # target_directory = "/home/user/datasets/images"           (macOS / Linux)
    
    target_directory = "./python/dataset/sr_yolo_filtered"  # Change this to your target directory
    
    change_classes_to_one(target_directory)