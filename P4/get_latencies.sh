
for f in `ls`; do
  #sed -i '/^$/d' "$f"
  sed -i '1,2d' "$f"
  sed -i '103,$d' "$f" 
  awk '{print $21}' "$f" | sed 's/\.$//' > "$f.tmp" && mv "$f.tmp" "$f"
done

