"""Copy the alphabounce.com page graphics the SDL3 port uses into assets/site/.

usage: python3 site_assets.py <path to Alphabounce folder> <assets dir>
"""
import glob
import os
import shutil
import sys

DESIGN = ['core_center.png', 'core_center_en.png', 'core_center_de.png', 'core_center_es.png', 'core_planet.jpg',
          'tab_on.gif', 'tab_off.gif', 'help.gif', 'help_en.gif', 'help_de.gif', 'help_es.gif',
          'button_small.gif', 'button_small_hover.gif']
ICONS = ['small_mission.gif', 'small_post.gif', 'small_money.gif', 'small_fuel.gif', 'small_fuel_free.gif',
         'small_fuel_empty.gif', 'small_pad.gif', 'small_earth.gif', 'small_bricks.gif', 'small_lock.gif',
         'faq_death.gif', 'faq_pad.gif', 'faq_shop.gif', 'large_planet.gif'] + \
        ['rank_%d.gif' % i for i in range(13)]


def main():
    base, out = sys.argv[1], os.path.join(sys.argv[2], 'site')
    web = os.path.join(base, 'web', 'img')
    os.makedirs(os.path.join(out, 'vigs'), exist_ok=True)
    for f in DESIGN:
        shutil.copy(os.path.join(web, 'design', f), os.path.join(out, f))
    for f in ICONS:
        shutil.copy(os.path.join(web, 'icons', f), os.path.join(out, f))
    n = 0
    for f in glob.glob(os.path.join(web, 'vigs', '*.png')):
        shutil.copy(f, os.path.join(out, 'vigs', os.path.basename(f)))
        n += 1
    print('copied %d page images and %d vignettes to %s' % (len(DESIGN) + len(ICONS), n, out))


if __name__ == '__main__':
    main()
