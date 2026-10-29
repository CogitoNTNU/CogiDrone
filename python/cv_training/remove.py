from pathlib import Path

def delete_human_detections(root_dir="."):
    target_suffix = "_human_detections.jpg"
    root_path = Path(root_dir)
    
    # rglob('*_human_detections.jpg') searches recursively
    deleted_count = 0
    for file_path in root_path.rglob(f"*{target_suffix}"):
        if file_path.is_file():
            try:
                file_path.unlink()
                print(f"Deleted: {file_path}")
                deleted_count += 1
            except Exception as e:
                print(f"Error deleting {file_path}: {e}")
                
    print(f"\nDone! Total files deleted: {deleted_count}")

if __name__ == "__main__":
    # Change "." to a specific directory path if you don't want to run it in the current folder
    delete_human_detections(".")