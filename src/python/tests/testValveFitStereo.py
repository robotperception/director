import os

from director.consoleapp import ConsoleApp
from director import ioutils
from director import segmentation
from director import segmentationroutines
from director import applogic
from director import visualization as vis
from director import drcargs

from director import roboturdf


app = ConsoleApp()

# create a view
view = app.createView()
segmentation._defaultSegmentationView = view
segmentation.initAffordanceManager(view)

robot_config = drcargs.getRobotConfig()
urdf_path = os.path.join(robot_config.dirname, robot_config['urdfConfig']['default'])
robotStateModel, robotStateJointController = roboturdf.loadRobotModel('robot state model', view, parent='sensors', urdfFile = urdf_path, color=roboturdf.getRobotGrayColor(), visible=True)
segmentationroutines.SegmentationContext.initWithRobot(robotStateModel)

# load poly data
dataDir = app.getTestingDataDirectory()
polyData = ioutils.readPolyData(os.path.join(dataDir, 'misc/valve/valve-sparse-stereo.pcd'))
vis.showPolyData(polyData, 'pointcloud snapshot original', colorByName='rgb_colors')
polyData = segmentationroutines.sparsifyStereoCloud( polyData )
vis.showPolyData(polyData, 'pointcloud snapshot')

# fit valve and lever
segmentation.segmentValveWallAuto(.2, mode='valve', removeGroundMethod=segmentation.removeGroundSimple )

if app.getTestingInteractiveEnabled():

    v = applogic.getCurrentView()
    v.camera().SetPosition([3,3,3])
    v.camera().SetFocalPoint([0,0,0])

    view.show()
    app.showObjectModel()
    app.start()
