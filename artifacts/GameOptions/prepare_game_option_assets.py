from pathlib import Path
from PIL import Image


ROOT = Path(__file__).resolve().parent
OUT = ROOT / "production"
OUT.mkdir(exist_ok=True)


def alpha_bbox(image):
	return image.getchannel("A").getbbox()


def crop_alpha(image, pad=2):
	bbox = alpha_bbox(image)
	if bbox is None:
		raise RuntimeError("image contains no visible pixels")
	left, top, right, bottom = bbox
	left = max(0, left - pad)
	top = max(0, top - pad)
	right = min(image.width, right + pad)
	bottom = min(image.height, bottom + pad)
	return image.crop((left, top, right, bottom))


def x_components(image, min_gap=12):
	alpha = image.getchannel("A")
	columns = []
	for x in range(image.width):
		columns.append(alpha.crop((x, 0, x + 1, image.height)).getbbox() is not None)

	runs = []
	start = None
	last_visible = None
	for x, visible in enumerate(columns):
		if visible:
			if start is None:
				start = x
			last_visible = x
		elif start is not None and x - last_visible >= min_gap:
			runs.append((start, last_visible + 1))
			start = None
			last_visible = None
	if start is not None:
		runs.append((start, last_visible + 1))
	return runs


def horizontal_sheet(source_name, output_name, count, cell_size):
	image = Image.open(ROOT / source_name).convert("RGBA")
	runs = x_components(image)
	if len(runs) != count:
		raise RuntimeError(f"{source_name}: expected {count} components, found {len(runs)}: {runs}")

	cell_w, cell_h = cell_size
	sheet = Image.new("RGBA", (cell_w * count, cell_h), (0, 0, 0, 0))
	for index, (left, right) in enumerate(runs):
		part = crop_alpha(image.crop((left, 0, right, image.height)), 1)
		part.thumbnail((cell_w - 4, cell_h - 4), Image.Resampling.LANCZOS)
		x = index * cell_w + (cell_w - part.width) // 2
		y = (cell_h - part.height) // 2
		sheet.alpha_composite(part, (x, y))
	sheet.save(OUT / output_name, optimize=True)


def single(source_name, output_name, size):
	image = crop_alpha(Image.open(ROOT / source_name).convert("RGBA"), 1)
	image = image.resize(size, Image.Resampling.LANCZOS)
	image.save(OUT / output_name, optimize=True)


single("game-options-window-v2.png", "window.png", (640, 600))
horizontal_sheet("game-options-tabs.png", "tabs.png", 4, (200, 64))
horizontal_sheet("game-options-close.png", "close.png", 4, (56, 56))
single("game-options-section-header.png", "section-header.png", (560, 44))
horizontal_sheet("game-options-checkbox.png", "checkbox.png", 4, (40, 40))
horizontal_sheet("game-options-dropdown.png", "dropdown.png", 4, (300, 44))
single("game-options-dropdown-list.png", "dropdown-list.png", (300, 220))
horizontal_sheet("game-options-dropdown-row.png", "dropdown-row.png", 4, (300, 40))
horizontal_sheet("game-options-effect-selector.png", "effect-selector.png", 5, (72, 56))
horizontal_sheet("game-options-action-button.png", "action-button.png", 4, (200, 56))
single("game-options-confirmation-panel.png", "confirmation-panel.png", (480, 290))


def prepare_volume_slider():
	image = Image.open(ROOT / "game-options-volume-slider.png").convert("RGBA")
	track = crop_alpha(image.crop((0, 120, image.width, 270)), 1).resize((600, 32), Image.Resampling.LANCZOS)
	fill = crop_alpha(image.crop((0, 280, image.width, 430)), 1).resize((600, 24), Image.Resampling.LANCZOS)
	thumb_area = image.crop((0, 440, image.width, image.height))
	runs = x_components(thumb_area)
	if len(runs) != 4:
		raise RuntimeError(f"volume thumbs: expected 4 components, found {len(runs)}")
	thumbs = Image.new("RGBA", (48 * 4, 64), (0, 0, 0, 0))
	for index, (left, right) in enumerate(runs):
		part = crop_alpha(thumb_area.crop((left, 0, right, thumb_area.height)), 1)
		part.thumbnail((42, 60), Image.Resampling.LANCZOS)
		thumbs.alpha_composite(part, (index * 48 + (48 - part.width) // 2, (64 - part.height) // 2))
	track.save(OUT / "volume-track.png", optimize=True)
	fill.save(OUT / "volume-fill.png", optimize=True)
	thumbs.save(OUT / "volume-thumb.png", optimize=True)


def prepare_scrollbar():
	image = Image.open(ROOT / "game-options-scrollbar.png").convert("RGBA")
	runs = x_components(image)
	if len(runs) != 5:
		raise RuntimeError(f"scrollbar: expected 5 component columns, found {len(runs)}")

	track = crop_alpha(image.crop((runs[0][0], 0, runs[0][1], image.height)), 1).resize((28, 240), Image.Resampling.LANCZOS)
	thumbs = Image.new("RGBA", (40 * 3, 96), (0, 0, 0, 0))
	for index, (left, right) in enumerate(runs[1:4]):
		part = crop_alpha(image.crop((left, 0, right, image.height)), 1)
		part.thumbnail((36, 92), Image.Resampling.LANCZOS)
		thumbs.alpha_composite(part, (index * 40 + (40 - part.width) // 2, (96 - part.height) // 2))

	arrow_column = image.crop((runs[4][0], 0, runs[4][1], image.height))
	alpha = arrow_column.getchannel("A")
	visible_rows = [alpha.crop((0, y, arrow_column.width, y + 1)).getbbox() is not None for y in range(arrow_column.height)]
	yruns = []
	start = None
	for y, visible in enumerate(visible_rows + [False]):
		if visible and start is None:
			start = y
		elif not visible and start is not None:
			yruns.append((start, y))
			start = None
	if len(yruns) != 2:
		raise RuntimeError(f"scrollbar arrows: expected 2 components, found {len(yruns)}")
	arrows = Image.new("RGBA", (48 * 2, 48), (0, 0, 0, 0))
	for index, (top, bottom) in enumerate(yruns):
		part = crop_alpha(arrow_column.crop((0, top, arrow_column.width, bottom)), 1)
		part.thumbnail((44, 44), Image.Resampling.LANCZOS)
		arrows.alpha_composite(part, (index * 48 + (48 - part.width) // 2, (48 - part.height) // 2))

	track.save(OUT / "scrollbar-track.png", optimize=True)
	thumbs.save(OUT / "scrollbar-thumb.png", optimize=True)
	arrows.save(OUT / "scrollbar-arrow.png", optimize=True)


prepare_volume_slider()
prepare_scrollbar()

print(f"Prepared assets in {OUT}")
