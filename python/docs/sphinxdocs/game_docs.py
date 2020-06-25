import os
import logging
from collections import defaultdict
from pathlib import Path
import textwrap
import yaml
import gc
import numpy as np

from griddly import GriddlyLoader, gd
from griddly.RenderTools import RenderToFile


class GamesToSphix():

    def __init__(self):
        logging.basicConfig(level=logging.DEBUG)
        self._logger = logging.getLogger('Game Doc Generator')
        self._env_names = set()

        self._observer_types = [
            gd.ObserverType.SPRITE_2D,
            gd.ObserverType.BLOCK_2D
        ]

    def _generate_action_description(self, actions):
        sphinx_string = ''
        return sphinx_string

    def _generate_object_tile_images(self, objects, doc_path, game_name, gdy_file):

        # img/sokoban-wall-x.png
        tile_images = defaultdict(dict)

        # load a simple griddly env with each tile printed
        loader = GriddlyLoader()
        renderer = RenderToFile()

        level_string = ''
        for i, object in enumerate(objects):
            if 'MapCharacter' in object:
                name = object['Name']
                level_string += object['MapCharacter']

        for observer_type in self._observer_types:
            game_description = loader.load_game_description(gdy_file)
            grid = game_description.load_level_string(f'{level_string}\n')

            player_count = grid.get_player_count()
            tileSize = grid.get_tile_size()

            game = grid.create_game(observer_type)

            players = []
            for p in range(player_count):
                players.append(game.register_player(f'P{p}', observer_type))

            observer_type_string = self._get_observer_type_string(observer_type)

            game.init()
            game.reset()
            rendered_sprite_map = np.array(game.observe(), copy=False)

            i = 0
            for object in objects:
                if 'MapCharacter' in object:
                    name = object['Name']
                    relative_image_path = os.path.join('img', f'{game_name.replace(" ","_")}-object-{observer_type_string}-{name}.png')
                    doc_image_path = os.path.join(doc_path, relative_image_path)

                    single_sprite = rendered_sprite_map[:, i * tileSize:i * tileSize + tileSize, :]
                    tile_images[observer_type_string][name] = relative_image_path
                    renderer.render(single_sprite, doc_image_path)
                    i += 1

            # We are creating loads of game instances. this forces the release of vulkan resources before the python GC
            game.release()

        return tile_images

    def _generate_object_description(self, objects, doc_path, game_name, gdy_file):

        sphinx_string = ''

        tile_images = self._generate_object_tile_images(objects, doc_path, game_name, gdy_file)

        key_table_name_header = '   * - Name ->\n'
        key_table_mapchar_header = '   * - Map Char ->\n'
        key_table_render_row = defaultdict(lambda: '')

        sphinx_string += '.. list-table:: Tiles\n   :header-rows: 2\n\n'

        for object in objects:
            name = object['Name']
            map_character = object['MapCharacter'] if 'MapCharacter' in object else None

            if map_character is not None:
                key_table_name_header += f'     - {name}\n'
                key_table_mapchar_header += f'     - {map_character}\n'
                for observer_type in self._observer_types:
                    observer_type_string = self._get_observer_type_string(observer_type)
                    key_table_render_row[observer_type_string] += f'     - .. image:: {tile_images[observer_type_string][name]}\n'

        sphinx_string += key_table_name_header
        sphinx_string += key_table_mapchar_header
        for observer_type in self._observer_types:
            observer_type_string = self._get_observer_type_string(observer_type)
            sphinx_string += f'   * - {observer_type_string}\n{key_table_render_row[observer_type_string]}'

        sphinx_string += '\n\n'
        return sphinx_string

    def _generate_level_images(self, game_name, num_levels, doc_path, gdy_file):

        # load a simple griddly env with each tile printed
        loader = GriddlyLoader()

        renderer = RenderToFile()

        level_images = defaultdict(dict)

        for level in range(num_levels):
            for observer_type in self._observer_types:
                observer_type_string = self._get_observer_type_string(observer_type)
                game_description = loader.load_game_description(gdy_file)
                grid = game_description.load_level(level)

                player_count = grid.get_player_count()

                game = grid.create_game(observer_type)

                players = []
                for p in range(player_count):
                    players.append(game.register_player(f'P{p}', observer_type))

                game.init()
                game.reset()
                rendered_level = np.array(game.observe(), copy=False)

                relative_image_path = os.path.join('img',
                                                   f'{game_name.replace(" ", "_")}-level-{observer_type_string}-{level}.png')
                doc_image_path = os.path.join(doc_path, relative_image_path)
                renderer.render(rendered_level, doc_image_path)
                level_images[observer_type_string][level] = relative_image_path

                # We are creating loads of game instances. this forces the release of vulkan resources before the python GC
                game.release()

        return level_images

    def _generate_levels_description(self, environment, doc_path, gdy_file):

        game_name = environment['Name']
        num_levels = len(environment['Levels'])

        sphinx_string = ''

        level_images = self._generate_level_images(game_name, num_levels, doc_path, gdy_file)

        level_table_header = '.. list-table:: Levels\n   :header-rows: 1\n\n'
        level_table_header += '   * - \n'
        for observer_type in self._observer_types:
            observer_type_string = self._get_observer_type_string(observer_type)
            level_table_header += f'     - {observer_type_string}\n'

        level_table_string = ''
        for level in range(num_levels):
            level_table_string += f'   * - {level}\n'
            for observer_type in self._observer_types:
                observer_type_string = self._get_observer_type_string(observer_type)

                level_image = level_images[observer_type_string][level]
                level_table_string += f'     - .. thumbnail:: {level_image}\n'

        level_table_string += '\n'

        sphinx_string += level_table_header
        sphinx_string += level_table_string

        return sphinx_string



    def _generate_game_docs(self, directory_path, doc_path, gdy_file):
        full_gdy_path = os.path.join(directory_path, gdy_file)

        sphinx_string = ''
        with open(full_gdy_path, 'r') as game_description_yaml:
            yaml_string = game_description_yaml.read()
            game_description = yaml.load(yaml_string)
            environment = game_description['Environment']
            game_name = environment["Name"]

            description = environment["Description"] if "Description" in environment else "No Description"

            if game_name not in self._env_names:
                self._env_names.add(game_name)
            else:
                raise NameError("Cannot have GDY games with the same names")

            self._logger.debug(f'Game description loaded: {game_name}')

            sphinx_string += game_name + '\n'
            sphinx_string += '=' * len(game_name) + '\n\n'

            sphinx_string += 'Description\n'
            sphinx_string += '-------------\n\n'

            sphinx_string += f'{description}\n\n'

            sphinx_string += 'Objects\n'
            sphinx_string += '-------\n\n'

            sphinx_string += self._generate_object_description(game_description['Objects'], doc_path, game_name,
                                                               full_gdy_path)

            sphinx_string += 'Levels\n'
            sphinx_string += '---------\n\n'

            sphinx_string += self._generate_levels_description(environment, doc_path, full_gdy_path)

            sphinx_string += 'YAML\n'
            sphinx_string += '----\n\n'
            sphinx_string += '.. code-block:: YAML\n\n'
            sphinx_string += f'{textwrap.indent(yaml_string, "   ")}\n\n'

        generated_game_doc_filename = f'{doc_path}/{game_name}.rst'
        with open(generated_game_doc_filename, 'w') as f:
            f.write(sphinx_string)

        return generated_game_doc_filename

    def generate(self, gdy_directory, title, doc_directory, directories, filenames):
        index_sphinx_string = ''

        doc_fullpath = os.path.realpath(f'../../../docs/games{doc_directory}')

        index_sphinx_string += f'{title}\n'
        index_sphinx_string += '=' * len(title) + '\n\n'

        index_sphinx_string += '.. toctree:: \n\n'

        doc_path = Path(doc_fullpath)
        doc_path.mkdir(parents=True, exist_ok=True)

        if len(filenames) > 0:
            img_path = Path(f'{doc_fullpath}/img')
            img_path.mkdir(parents=True, exist_ok=True)
            for filename in filenames:
                if filename.endswith('.yaml'):
                    doc_filename = self._generate_game_docs(gdy_directory, doc_fullpath, gdy_file=filename)
                    index_sphinx_string += f'   {doc_filename.replace(f"{doc_fullpath}/","")}\n'
                else:
                    self._logger.warning(f'Ignoring file {filename} as it does not end in .yaml')


        for dir in directories:
            index_sphinx_string += f'   {dir}/index.rst\n'

        with open(f'{doc_fullpath}/index.rst', 'w') as f:
            f.write(index_sphinx_string)

    def _get_observer_type_string(self, observer_type):
        if observer_type is gd.ObserverType.SPRITE_2D:
            return "SPRITE_2D"
        elif observer_type is gd.ObserverType.BLOCK_2D:
            return "BLOCK_2D"
        else:
            return "Unknown"


if __name__ == '__main__':
    games_dir = os.path.realpath('../../../resources/games')
    for directory_path, directory_names, filenames in os.walk(games_dir):
        generator = GamesToSphix()
        docs_path = directory_path.replace(games_dir, '')
        title = docs_path if len(docs_path) > 0 else 'Games'

        title = title.lstrip('/')
        generator.generate(directory_path, title, docs_path, directory_names, filenames)