#xeyes &
feh --bg-tile /home/mace/Pictures/wallpaper/gray.png &
#exec crescendo &
exec xfce4-terminal &
exec dunst &
#exec valgrind --leak-check=full --show-leak-kinds=all --log-file=valgrind.log /home/mace/projects/notewm/wm
#exec gdb -ex run ./notewm
exec polybar bar1 --config=~/.config/polybar/config &
exec ./notewm
#exec gnome-session-custom-session
#exec ./notewm
