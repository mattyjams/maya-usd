# Copyright 2022 Autodesk
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import maya.cmds as cmds
import maya.mel as mel

def register(key, value):
    registerPluginResource('mayaUsdLib', key, value)

def getMayaUsdLibString(key):
    return getPluginResource('mayaUsdLib', key)

def mayaUsdLibRegisterStrings():
    # This function is called from the equivalent MEL proc
    # with the same name. The strings registered here and all the
    # ones registered from the MEL proc can be used in either
    # MEL or python.

    # Any python strings from MayaUsd lib go here.

    # ae_template.py
    register('kKindMetadataAnn', 'Kind is a type of metadata (a pre-loaded string value) used to classify prims in USD. Set the classification value from the dropdown to assign a kind category to a prim. Set a kind value to activate selection by kind.')
    register('kActiveMetadataAnn', "If selected, the prim is set to active and contributes to the composition of a stage. If a prim is set to inactive, it doesn't contribute to the composition of a stage (it gets striked out in the Outliner and is deactivated from the Viewport).")
    register('kInstanceableMetadataAnn', 'If selected, instanceable is set to true for the prim and the prim is considered a candidate for instancing. If deselected, instanceable is set to false.')
    register('kErrorAttributeMustBeArray', '"^1s" must be an array!')
    register('kMenuCopyValue', 'Copy Attribute Value')
    register('kMenuPrintValue', 'Print to Script Editor')
    register('kLabelUnusedTransformAttrs', 'Unused')
    register('kLabelMetadata', 'Metadata')
    register('kLabelAppliedSchemas', 'Applied Schemas')
    register('kOpenImage', 'Open')
    register('kLabelMaterial', 'Material')
    register('kLabelAssignedMaterial', 'Assigned Material')
    register('kAnnShowMaterialInLookdevx', 'Show in LookdevX')
    register('kLabelInheritedMaterial', 'Inherited Material')
    register('kLabelInheritedFromPrim', 'Inherited from Prim')
    register('kLabelInheriting', 'inheriting')
    register('kTooltipInheritingOverDirect', 'This material is being over-ridden due to the strength setting on an ancestor')
    register('kTooltipInheriting', 'This material is inherited from an ancestor')
    register('kLabelMaterialStrength', 'Strength')
    register('kLabelWeakerMaterial', 'Weaker than descendants')
    register('kLabelStrongerMaterial', 'Stronger than descendants')
    register('kTooltipInheritedStrength', 'This setting cannot be changed on this prim due to the strength setting on an ancestor')
    register('kLabelMaterialNewTab', 'New Tab...')
    register('kUseOutlinerColorAnn', 'Apply the Outliner color to the display of the prim name in the Outliner.')
    register('kOutlinerColorAnn', 'The color of the text displayed in the Outliner.')

    # mayaUsdAddMayaReference.py
    register('kErrorGroupPrimExists', 'Group prim "^1s" already exists under "^2s". Choose prim name other than "^1s" to proceed.')
    register('kErrorCannotAddToProxyShape', 'Cannot add Maya Reference node to ProxyShape with Variant Set unless grouped. Enable Group checkbox to proceed.')
    register('kErrorMayaRefPrimExists', 'Maya Reference prim "^1s" already exists under "^2s". Choose Maya Reference prim name other than "^1s" to proceed.')
    register('kErrorCreatingGroupPrim', 'Cannot create group prim under "^1s". Ensure target layer is editable and "^2s" can be added to "^1s".')
    register('kErrorCreatingMayaRefPrim', 'Cannot create MayaReference prim under "^1s". Ensure target layer is editable and "^2s" can be added to "^1s".')
    register('kErrorCreateVariantSet', 'Cannot create Variant Set on prim at path "^1s". Ensure target layer is editable and "^2s" can be added to "^3s".')

    # mayaUsdCacheMayaReference.py
    register('kButtonNewChildPrim', 'New Child Prim')
    register('kButtonNewChildPrimToolTip', 'If selected, your Maya reference will be defined in a new child prim. This will enable\nyou to work with your Maya reference and its USD cache side-by-side.')
    register('kCacheFileWillAppear', 'Cache file will\nappear on parent\nprim:')
    register('kCacheMayaRefCache', 'Cache')
    register('kCacheMayaRefOptions', 'Cache File Options')
    register('kCacheMayaRefUsdHierarchy', 'Author Cache File to USD')
    register('kCaptionCacheToUsd', 'Cache to USD')
    register('kErrorCacheToUsdFailed', 'Cache to USD failed for "^1s".')
    register('kMenuPrepend', 'Prepend')
    register('kMenuAppend', 'Append')
    register('kMenuPayload', 'Payload')
    register('kMenuReference', 'Reference')
    register('kOptionAsUSDReference', 'Composition Arc:')
    register('kOptionAsUSDReferenceToolTip', '<p>Choose the type of USD Reference composition arc for your Maya Reference:<br><br><b>Payloads</b> are a type of reference. They are recorded, but not traversed in the scene hierarchy. Select this arc if your goal is to manually construct<br>a "working set" that is a subset of an entire scene, in which only parts of the scene are required/loaded. Note: payloads are<br>weaker than direct references in any given LayerStack.<br><br><b>References</b> are general and can be used to compose smaller units of scene description into larger aggregates, building up a namespace that<br>includes the "encapsulated" result of composing the scene description targeted by a reference. Select this arc if your goal is not to unload your<br>references.</p>')
    register('kOptionAsUSDReferenceStatusMsg', 'Choose the type of USD Reference composition arc for your Maya Reference.')
    register('kOptionListEditedAs', 'List Edited As')
    register('kOptionLoadPayload', 'Load Payload:')
    register('kLoadPayloadAnnotation', 'If selected, all existing payloads on the prim will be unchanged and new payloads will be loaded as well. When deselected, all payloads on the prim will be unloaded.')
    register('kTextDefineIn', 'Define in:')
    register('kTextVariant', 'Variant')
    register('kTextVariantToolTip','If selected, your Maya reference will be defined in a variant. This will enable your prim to\nhave 2 variants you can switch between in the Outliner; the Maya reference and its USD cache.')

    register('kAddRefOrPayloadPrimPathToolTip',
        'Leave this field blank to use the default prim as your prim path (only viable if your file has a default prim).\n' +
        'Specifying a prim path will make an explicit reference to a prim.\n' +
        'If there is no default prim and no prim path is specified, no prim will be referenced.')
    
    register('kAddRefOrPayloadPrimPathLabel', 'Prim Path')
    register('kAddRefOrPayloadPrimPathPlaceHolder', ' (Default Prim)')
    register('kAddRefOrPayloadPrimPathHelpLabel', 'Help on Select a Prim for Reference')
    register('kAddRefOrPayloadPrimPathTitle', 'Select a Prim to Reference')
    register('kAddRefOrPayloadSelectLabel', 'Select')

    # mayaUsdClearRefsOrPayloadsOptions.py
    register('kClearRefsOrPayloadsOptionsTitle', 'Clear All USD References/Payloads')
    register('kClearRefsOrPayloadsOptionsMessage', 'Clear all references/payloads on %s?')
    register('kClearButton', 'Clear')
    register('kCancelButton', 'Cancel')
    register('kAllRefsLabel', 'All References')
    register('kAllRefsTooltip', 'Clear all references on the prim.')
    register('kAllPayloadsLabel', 'All Payloads')
    register('kAllPayloadsTooltip', 'Clear all payloads on the prim.')

    # mayaUsdMergeToUSDOptions.py
    register('kMergeToUSDOptionsTitle', 'Merge Maya Edits to USD Options')
    register('kMergeButton', 'Merge')
    register('kApplyButton', 'Apply')
    register('kCloseButton', 'Close')
    register('kEditMenu', 'Edit')
    register('kSaveSettingsMenuItem', 'Save Settings')
    register('kResetSettingsMenuItem', 'Reset Settings')
    register('kHelpMenu', 'Help')
    register('kHelpMergeToUSDOptionsMenuItem', 'Help on Merge Maya Edits to USD Options')

    # mayaUsdDuplicateAsUsdDataOptions.py
    register('kDuplicateAsUsdDataOptionsTitle', 'Duplicate As USD Data Options')
    register('kHelpDuplicateAsUsdDataOptionsMenuItem', 'Help on Duplicate As USD Data Options')

    # mayaUsdMergeToUsd.py
    register('kErrorMergeToUsdMenuItem', 'Could not create menu item for merge to USD')
    register('kMenuCacheToUsd', 'Cache to USD...')
    register('kMenuMergeMayaEdits', 'Merge Maya Edits to USD');

def registerPluginResource(pluginId, stringId, resourceStr):
    '''See registerPluginResource.mel in Maya.

    Unfortunately there is no equivalent python version of this MEL proc
    so we created our own version of it here.'''

    fullId = 'p_%s.%s' % (pluginId, stringId)
    if cmds.displayString(fullId, exists=True):
        # Issue warning if the string is already registered.
        msgFormat = mel.eval('uiRes("m_registerPluginResource.kNotRegistered")')
        msg = cmds.format(msgFormat, stringArg=(pluginId, stringId))
        cmds.warning(msg)
        # Replace the string's value
        cmds.displayString(fullId, replace=True, value=resourceStr)
    else:
        # Set the string's default value.
        cmds.displayString(fullId, value=resourceStr)

def getPluginResource(pluginId, stringId):
    '''See getPluginResource.mel in Maya.

    Unfortunately there is no equivalent python version of this MEL proc
    so we created our own version of it here.'''

    # Form full id string.
    # Plugin string id's are prefixed with "p_".
    fullId = 'p_%s.%s' % (pluginId, stringId)
    if cmds.displayString(fullId, exists=True):
        dispStr = cmds.displayString(fullId, query=True, value=True)
        return dispStr
    else:
        msgFormat = mel.eval('uiRes("m_getPluginResource.kLookupFailed")')
        msg = cmds.format(msgFormat, stringArg=(pluginId, stringId))
        cmds.error(msg)
