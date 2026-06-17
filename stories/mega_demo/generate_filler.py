import sys
import os

def main():
    os.makedirs("c:/Users/tomgr/dev/eenk/stories/mega_demo", exist_ok=True)
    lines = []
    lines.append("=== filler_story ===")
    lines.append("-> filler_chapter_0")
    
    # Generate 8,000 chapters to ensure the file is large (around 6.5MB)
    # but stays safely under the ESP32-C3's hard 8MB flash memory-mapping limit.
    for i in range(8000):
        lines.append(f"=== filler_chapter_{i} ===")
        for j in range(3):
            lines.append(f"Chapter {i}, short line {j}. Quick text to avoid long scrolling.")
        
        if i < 7999:
            lines.append(f"+ [Proceed to Chapter {i+1}] -> filler_chapter_{i+1}")
            lines.append(f"+ [Take a detour] -> filler_chapter_{i}_detour")
            lines.append(f"+ [Return to main menu] -> main_menu")
            
            lines.append(f"=== filler_chapter_{i}_detour ===")
            lines.append("You took a quick detour. Nothing here.")
            lines.append(f"+ [Back to path (Chapter {i+1})] -> filler_chapter_{i+1}")
            lines.append(f"+ [Return to main menu] -> main_menu")
        else:
            lines.append("You have reached the very end of the mega story.")
            lines.append("-> main_menu")
            
    with open("c:/Users/tomgr/dev/eenk/stories/mega_demo/filler.ink", "w") as f:
        f.write("\n".join(lines))

if __name__ == "__main__":
    main()
