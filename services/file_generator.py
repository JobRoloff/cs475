from pathlib import Path
class FileGenerator():
    def export_data(self, data, filepath: str):
        path = Path(filepath)
        ext = path.suffix.lower()
        
        if ext == ".md":
            with open(f"{filepath}", "w", encoding="utf-8") as file:
                file.write(data)
        else:
            raise ValueError(f"Unsupported file format: {ext}")