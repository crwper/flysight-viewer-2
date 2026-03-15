"""
Rewrite dockLayout in .fvprofile files to use clean split ratios.

Target layout (1438x803 usable area, 5px splitter gaps):
  Root (vertical 50/50)
  ├── Top row (horizontal 20/60/20)
  │   ├── Marker Selection tabs
  │   ├── Plots
  │   └── Legend
  └── Bottom row (horizontal 50/50)
      ├── Video
      └── Map
"""

import json
import base64
import glob
import os

SPLITTER_GAP = 5
TOTAL_W = 1438
TOTAL_H = 803

PROFILE_DIR = os.path.join(os.path.dirname(__file__),
                           '..', 'src', 'resources', 'profiles')


def compute_geometries():
    """Compute pixel geometries for the clean layout."""
    # Vertical split: 50/50
    avail_h = TOTAL_H - SPLITTER_GAP  # 798
    top_h = avail_h // 2              # 399
    bot_h = avail_h - top_h           # 399

    # Top row: horizontal 20/60/20 (3 panels, 2 gaps)
    avail_w_top = TOTAL_W - 2 * SPLITTER_GAP  # 1428
    left_w = round(avail_w_top * 0.2)    # 286
    center_w = round(avail_w_top * 0.6)  # 857
    right_w = avail_w_top - left_w - center_w  # 285

    # Bottom row: horizontal 50/50 (2 panels, 1 gap)
    avail_w_bot = TOTAL_W - SPLITTER_GAP  # 1433
    bot_left_w = avail_w_bot // 2         # 716
    bot_right_w = avail_w_bot - bot_left_w  # 717

    bot_y = top_h + SPLITTER_GAP  # 404

    return {
        'top_h': top_h,
        'bot_h': bot_h,
        'bot_y': bot_y,
        'left_w': left_w,
        'center_w': center_w,
        'center_x': left_w + SPLITTER_GAP,
        'right_w': right_w,
        'right_x': left_w + SPLITTER_GAP + center_w + SPLITTER_GAP,
        'bot_left_w': bot_left_w,
        'bot_right_w': bot_right_w,
        'bot_right_x': bot_left_w + SPLITTER_GAP,
    }


def find_frame_by_widget(frames, widget_name):
    """Find the frame ID that contains a given dock widget name."""
    for fid, frame in frames.items():
        if widget_name in frame['dockWidgets']:
            return fid
    return None


def find_frame_by_widgets(frames, widget_names):
    """Find the frame ID whose dockWidgets list matches (as a set)."""
    target = set(widget_names)
    for fid, frame in frames.items():
        if set(frame['dockWidgets']) == target:
            return fid
    return None


def make_geo(x, y, w, h):
    return {'x': x, 'y': y, 'width': w, 'height': h}


def update_layout(dock):
    g = compute_geometries()
    mw = dock['mainWindows'][0]
    msl = mw['multiSplitterLayout']
    frames = msl['frames']

    # Identify frame IDs by their dock widgets
    marker_id = find_frame_by_widgets(frames,
                                      ['Logbook', 'Marker Selection', 'Plot Selection'])
    plots_id = find_frame_by_widget(frames, 'Plots')
    legend_id = find_frame_by_widget(frames, 'Legend')
    video_id = find_frame_by_widget(frames, 'Video')
    map_id = find_frame_by_widget(frames, 'Map')

    # Update frame geometries
    frames[marker_id]['geometry'] = make_geo(0, 0, g['left_w'], g['top_h'])
    frames[plots_id]['geometry'] = make_geo(g['center_x'], 0,
                                            g['center_w'], g['top_h'])
    frames[legend_id]['geometry'] = make_geo(g['right_x'], 0,
                                             g['right_w'], g['top_h'])
    frames[video_id]['geometry'] = make_geo(0, g['bot_y'],
                                            g['bot_left_w'], g['bot_h'])
    frames[map_id]['geometry'] = make_geo(g['bot_right_x'], g['bot_y'],
                                          g['bot_right_w'], g['bot_h'])

    # Now update the layout tree.
    # Tree structure (all profiles share this):
    #   root (vert)
    #     child[0] (horiz) = top half
    #       child[0] (vert) = wrapper
    #         child[0] (horiz) = top-row container with 3 leaves
    #           leaf[0] = marker_id
    #           leaf[1] = plots_id
    #           leaf[2] = legend_id
    #     child[1] (horiz) = bottom half
    #       leaf[0] = video_id
    #       leaf[1] = map_id

    root = msl['layout']
    top_half = root['children'][0]
    bot_half = root['children'][1]

    # Navigate to the 3-leaf container
    top_wrapper_vert = top_half['children'][0]
    top_row_horiz = top_wrapper_vert['children'][0]
    leaves_top = top_row_horiz['children']
    leaves_bot = bot_half['children']

    # Map guest IDs to their desired geometry and percentage
    top_configs = [
        (marker_id, make_geo(0, 0, g['left_w'], g['top_h']), 0.2),
        (plots_id, make_geo(g['center_x'], 0, g['center_w'], g['top_h']), 0.6),
        (legend_id, make_geo(g['right_x'], 0, g['right_w'], g['top_h']), 0.2),
    ]
    bot_configs = [
        (video_id, make_geo(0, 0, g['bot_left_w'], g['bot_h']), 0.5),
        (map_id, make_geo(g['bot_right_x'], 0, g['bot_right_w'], g['bot_h']), 0.5),
    ]

    # Update top row leaves (match by guestId order)
    for leaf, (fid, geo, pct) in zip(leaves_top, top_configs):
        assert leaf['guestId'] == fid, \
            f"Expected {fid}, got {leaf['guestId']}"
        leaf['sizingInfo']['geometry'] = geo
        leaf['sizingInfo']['percentageWithinParent'] = pct

    # Update bottom row leaves
    for leaf, (fid, geo, pct) in zip(leaves_bot, bot_configs):
        assert leaf['guestId'] == fid, \
            f"Expected {fid}, got {leaf['guestId']}"
        leaf['sizingInfo']['geometry'] = geo
        leaf['sizingInfo']['percentageWithinParent'] = pct

    # Update container geometries and percentages
    top_row_horiz['sizingInfo']['geometry'] = make_geo(0, 0, TOTAL_W, g['top_h'])
    top_row_horiz['sizingInfo']['percentageWithinParent'] = 1.0

    top_wrapper_vert['sizingInfo']['geometry'] = make_geo(0, 0, TOTAL_W, g['top_h'])
    top_wrapper_vert['sizingInfo']['percentageWithinParent'] = 1.0

    top_half['sizingInfo']['geometry'] = make_geo(0, 0, TOTAL_W, g['top_h'])
    top_half['sizingInfo']['percentageWithinParent'] = 0.5

    bot_half['sizingInfo']['geometry'] = make_geo(0, g['bot_y'], TOTAL_W, g['bot_h'])
    bot_half['sizingInfo']['percentageWithinParent'] = 0.5

    root['sizingInfo']['geometry'] = make_geo(0, 0, TOTAL_W, TOTAL_H)

    return dock


def process_profile(path):
    with open(path, 'r', encoding='utf-8') as f:
        profile = json.load(f)

    encoded = profile['dockLayout']
    dock_json = base64.b64decode(encoded).decode('utf-8')
    dock = json.loads(dock_json)

    dock = update_layout(dock)

    new_json = json.dumps(dock, indent=4)
    profile['dockLayout'] = base64.b64encode(new_json.encode('utf-8')).decode('ascii')

    with open(path, 'w', encoding='utf-8') as f:
        json.dump(profile, f, indent=4)
        f.write('\n')

    print(f"Updated: {os.path.basename(path)}")


def main():
    profiles = sorted(glob.glob(os.path.join(PROFILE_DIR, '*.fvprofile')))
    if not profiles:
        print("No profiles found!")
        return
    for p in profiles:
        process_profile(p)

    # Print resulting layout for verification
    g = compute_geometries()
    print(f"\nLayout applied ({TOTAL_W}x{TOTAL_H}, {SPLITTER_GAP}px gaps):")
    print(f"  Top row (h={g['top_h']}): "
          f"MarkerSel w={g['left_w']} | "
          f"Plots w={g['center_w']} | "
          f"Legend w={g['right_w']}")
    print(f"  Bot row (h={g['bot_h']}, y={g['bot_y']}): "
          f"Video w={g['bot_left_w']} | "
          f"Map w={g['bot_right_w']}")


if __name__ == '__main__':
    main()
