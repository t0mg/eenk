import sys
import os

def main():
    os.makedirs("c:/Users/tomgr/dev/eenk/stories/mega_demo", exist_ok=True)
    lines = []
    lines.append("=== filler_story ===")
    lines.append("-> filler_chapter_0")
    
    # Generate 2,000 chapters to keep the number of knots/choices low.
    # Each knot/choice requires 8 bytes of heap RAM for state tracking (visit_count).
    # 2,000 chapters * 4 containers/chapter = 8,000 containers (~64KB heap), which easily fits in ESP32-C3 RAM.
    for i in range(2000):
        lines.append(f"=== filler_chapter_{i} ===")
        for j in range(3):
            lines.append(f"Chapter {i}, short line {j}. Quick text to avoid long scrolling.")
        
        if i < 1999:
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

    # Add a massive unreachable knot to pad out the binary size to 3-5MB
    # without creating new containers that would consume heap RAM.
    lines.append("=== filler_padding_bulk ===")
    for i in range(20000):
        lines.append(f"This is padding line {i} to increase the binary size for flash streaming tests. It lives safely in Flash and consumes no heap RAM because it is not a container or a choice.")
    lines.append("-> DONE")
            
    with open("c:/Users/tomgr/dev/eenk/stories/mega_demo/filler.ink", "w") as f:
        f.write("\n".join(lines))

if __name__ == "__main__":
    main()
