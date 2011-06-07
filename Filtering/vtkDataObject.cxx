/*=========================================================================

Program:   Visualization Toolkit
Module:    vtkDataObject.cxx

Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
All rights reserved.
See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkDataObject.h"

#include "vtkAlgorithmOutput.h"
#include "vtkDataSetAttributes.h"
#include "vtkExtentTranslator.h"
#include "vtkFieldData.h"
#include "vtkGarbageCollector.h"
#include "vtkInformation.h"
#include "vtkObjectFactory.h"
#include "vtkTrivialProducer.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkInformationDataObjectKey.h"
#include "vtkInformationDoubleKey.h"
#include "vtkInformationDoubleVectorKey.h"
#include "vtkInformationExecutivePortKey.h"
#include "vtkInformationExecutivePortVectorKey.h"
#include "vtkInformationIntegerKey.h"
#include "vtkInformationIntegerPointerKey.h"
#include "vtkInformationIntegerVectorKey.h"
#include "vtkInformationInformationVectorKey.h"
#include "vtkInformationStringKey.h"
#include "vtkInformationVector.h"
#include "vtkDataSetAttributes.h"

vtkStandardNewMacro(vtkDataObject);

vtkCxxSetObjectMacro(vtkDataObject,Information,vtkInformation);
vtkCxxSetObjectMacro(vtkDataObject,FieldData,vtkFieldData);

vtkInformationKeyMacro(vtkDataObject, DATA_TYPE_NAME, String);
vtkInformationKeyMacro(vtkDataObject, DATA_OBJECT, DataObject);
vtkInformationKeyMacro(vtkDataObject, DATA_EXTENT_TYPE, Integer);
vtkInformationKeyMacro(vtkDataObject, DATA_PIECE_NUMBER, Integer);
vtkInformationKeyMacro(vtkDataObject, DATA_NUMBER_OF_PIECES, Integer);
vtkInformationKeyMacro(vtkDataObject, DATA_NUMBER_OF_GHOST_LEVELS, Integer);
vtkInformationKeyMacro(vtkDataObject, DATA_RESOLUTION, Double);
vtkInformationKeyMacro(vtkDataObject, DATA_TIME_STEPS, DoubleVector);
vtkInformationKeyMacro(vtkDataObject, POINT_DATA_VECTOR, InformationVector);
vtkInformationKeyMacro(vtkDataObject, CELL_DATA_VECTOR, InformationVector);
vtkInformationKeyMacro(vtkDataObject, VERTEX_DATA_VECTOR, InformationVector);
vtkInformationKeyMacro(vtkDataObject, EDGE_DATA_VECTOR, InformationVector);
vtkInformationKeyMacro(vtkDataObject, FIELD_ARRAY_TYPE, Integer);
vtkInformationKeyMacro(vtkDataObject, FIELD_ASSOCIATION, Integer);
vtkInformationKeyMacro(vtkDataObject, FIELD_ATTRIBUTE_TYPE, Integer);
vtkInformationKeyMacro(vtkDataObject, FIELD_ACTIVE_ATTRIBUTE, Integer);
vtkInformationKeyMacro(vtkDataObject, FIELD_NAME, String);
vtkInformationKeyMacro(vtkDataObject, FIELD_NUMBER_OF_COMPONENTS, Integer);
vtkInformationKeyMacro(vtkDataObject, FIELD_NUMBER_OF_TUPLES, Integer);
vtkInformationKeyRestrictedMacro(vtkDataObject, FIELD_RANGE, DoubleVector, 2);
vtkInformationKeyRestrictedMacro(vtkDataObject, PIECE_FIELD_RANGE, DoubleVector, 2);
vtkInformationKeyMacro(vtkDataObject, FIELD_ARRAY_NAME, String);
vtkInformationKeyRestrictedMacro(vtkDataObject, PIECE_EXTENT, IntegerVector, 6);
vtkInformationKeyMacro(vtkDataObject, FIELD_OPERATION, Integer);
vtkInformationKeyRestrictedMacro(vtkDataObject, DATA_EXTENT, IntegerPointer, 6);
vtkInformationKeyRestrictedMacro(vtkDataObject, ORIGIN, DoubleVector, 3);
vtkInformationKeyRestrictedMacro(vtkDataObject, SPACING, DoubleVector, 3);
vtkInformationKeyMacro(vtkDataObject, DATA_GEOMETRY_UNMODIFIED, Integer);
vtkInformationKeyMacro(vtkDataObject, SIL, DataObject);


// Initialize static member that controls global data release 
// after use by filter
static int vtkDataObjectGlobalReleaseDataFlag = 0;

const char vtkDataObject
::AssociationNames[vtkDataObject::NUMBER_OF_ASSOCIATIONS][55] =
{
  "vtkDataObject::FIELD_ASSOCIATION_POINTS",
  "vtkDataObject::FIELD_ASSOCIATION_CELLS",
  "vtkDataObject::FIELD_ASSOCIATION_NONE",
  "vtkDataObject::FIELD_ASSOCIATION_POINTS_THEN_CELLS",
  "vtkDataObject::FIELD_ASSOCIATION_VERTICES",
  "vtkDataObject::FIELD_ASSOCIATION_EDGES",
  "vtkDataObject::FIELD_ASSOCIATION_ROWS"
};

//----------------------------------------------------------------------------
vtkDataObject::vtkDataObject()
{
  this->PipelineInformation = 0;

  this->Information = vtkInformation::New();

  // We have to assume that if a user is creating the data on their own,
  // then they will fill it with valid data.
  this->DataReleased = 0;

  this->FieldData = NULL;
  vtkFieldData *fd = vtkFieldData::New();
  this->SetFieldData(fd);
  fd->FastDelete();
}

//----------------------------------------------------------------------------
vtkDataObject::~vtkDataObject()
{
  this->SetPipelineInformation(0);
  this->SetInformation(0);
  this->SetFieldData(NULL);
}

//----------------------------------------------------------------------------
void vtkDataObject::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  if ( this->Information )
    {
    os << indent << "Information: " << this->Information << "\n";
    }
  else
    {
    os << indent << "Information: (none)\n";
    }

  os << indent << "Data Released: "
     << (this->DataReleased ? "True\n" : "False\n");
  os << indent << "Global Release Data: "
     << (vtkDataObjectGlobalReleaseDataFlag ? "On\n" : "Off\n");

  os << indent << "UpdateTime: " << this->UpdateTime << endl;

  os << indent << "Field Data:\n";
  this->FieldData->PrintSelf(os,indent.GetNextIndent());
}


//----------------------------------------------------------------------------
void vtkDataObject::SetPipelineInformation(vtkInformation* newInfo)
{
  vtkInformation* oldInfo = this->PipelineInformation;
  if(newInfo != oldInfo)
    {
    if(newInfo)
      {
      // Reference the new information.
      newInfo->Register(this);

      // Detach the output that used to be held by the new information.
      if(vtkDataObject* oldData = newInfo->Get(vtkDataObject::DATA_OBJECT()))
        {
        oldData->Register(this);
        oldData->SetPipelineInformation(0);
        oldData->UnRegister(this);
        }

      // Tell the new information about this object.
      newInfo->Set(vtkDataObject::DATA_OBJECT(), this);
      }

    // Save the pointer to the new information.
    this->PipelineInformation = newInfo;

    if(oldInfo)
      {
      // Remove the old information's reference to us.
      oldInfo->Set(vtkDataObject::DATA_OBJECT(), 0);

      // Remove our reference to the old information.
      oldInfo->UnRegister(this);
      }
    }
}

//----------------------------------------------------------------------------
vtkAlgorithmOutput* vtkDataObject::GetProducerPort()
{  
  // Make sure there is an executive.
  if(!this->GetExecutive())
    {
    vtkTrivialProducer* tp = vtkTrivialProducer::New();
    tp->SetOutput(this);
    tp->FastDelete();
    }

  // Get the port from the executive.
  return this->GetExecutive()->GetProducerPort(this);
}

//----------------------------------------------------------------------------
// Determine the modified time of this object
unsigned long int vtkDataObject::GetMTime()
{
  unsigned long result;

  result = vtkObject::GetMTime();
  if ( this->FieldData )
    {
    unsigned long mtime = this->FieldData->GetMTime();
    result = ( mtime > result ? mtime : result);
    }
  
  return result;
}

//----------------------------------------------------------------------------
void vtkDataObject::Initialize()
{
  if (this->FieldData)
    {
    this->FieldData->Initialize();
    }

  if (this->Information)
    {
    // Make sure the information is cleared.
    this->Information->Remove(DATA_PIECE_NUMBER());
    this->Information->Remove(DATA_NUMBER_OF_PIECES());
    this->Information->Remove(DATA_NUMBER_OF_GHOST_LEVELS());
    this->Information->Remove(DATA_TIME_STEPS());
    this->Information->Remove(DATA_RESOLUTION());
    }

  this->Modified();
}

//----------------------------------------------------------------------------
void vtkDataObject::SetGlobalReleaseDataFlag(int val)
{
  if (val == vtkDataObjectGlobalReleaseDataFlag)
    {
    return;
    }
  vtkDataObjectGlobalReleaseDataFlag = val;
}

//----------------------------------------------------------------------------
void vtkDataObject::CopyInformationToPipeline(vtkInformation *request, 
                                              vtkInformation *input,
                                              vtkInformation *output,
                                              int vtkNotUsed(forceCopy))
{
  // Set default pipeline information during a request for information.
  if(request->Has(vtkDemandDrivenPipeline::REQUEST_INFORMATION()))
    {
    // Copy point and cell data from the input if available.  Otherwise use our
    // current settings.

    if (input)
      {
      // copy point data.
      if (input->Has(POINT_DATA_VECTOR()))
        {
        output->CopyEntry(input, POINT_DATA_VECTOR(), 1);
        }
      // copy cell data.
      if (input && input->Has(CELL_DATA_VECTOR()))
        {
        output->CopyEntry(input, CELL_DATA_VECTOR(), 1);
        }
      // copy vertex data.
      if (input->Has(VERTEX_DATA_VECTOR()))
        {
        output->CopyEntry(input, VERTEX_DATA_VECTOR(), 1);
        }
      // copy edge data.
      if (input && input->Has(EDGE_DATA_VECTOR()))
        {
        output->CopyEntry(input, EDGE_DATA_VECTOR(), 1);
        }

      // copy the actual time
      if (input->Has(DATA_TIME_STEPS()))
        {
        output->CopyEntry(input, DATA_TIME_STEPS());
        }
      }
    }
  if(request->Has(vtkDemandDrivenPipeline::REQUEST_DATA()))
    {
    if (input)
      {
      if (input->Has(DATA_RESOLUTION()))
        {
        output->CopyEntry(input, DATA_RESOLUTION());
        }
      }
    }
}

//----------------------------------------------------------------------------
vtkInformation *vtkDataObject::GetActiveFieldInformation(vtkInformation *info, 
    int fieldAssociation, int attributeType)
{
  int i;
  vtkInformation *fieldDataInfo;
  vtkInformationVector *fieldDataInfoVector;
  
  if (fieldAssociation == FIELD_ASSOCIATION_POINTS)
    {
    fieldDataInfoVector = info->Get(POINT_DATA_VECTOR());
    }
  else if (fieldAssociation == FIELD_ASSOCIATION_CELLS)
    {
    fieldDataInfoVector = info->Get(CELL_DATA_VECTOR());
    }
  else if (fieldAssociation == FIELD_ASSOCIATION_VERTICES)
    {
    fieldDataInfoVector = info->Get(VERTEX_DATA_VECTOR());
    }
  else if (fieldAssociation == FIELD_ASSOCIATION_EDGES)
    {
    fieldDataInfoVector = info->Get(EDGE_DATA_VECTOR());
    }
  else
    {
    vtkGenericWarningMacro("Unrecognized field association!");
    return NULL;
    }

  if (!fieldDataInfoVector)
    {
    return NULL;
    }

  for (i = 0; i < fieldDataInfoVector->GetNumberOfInformationObjects(); i++)
    {
    fieldDataInfo = fieldDataInfoVector->GetInformationObject(i);
    if ( fieldDataInfo->Has(FIELD_ACTIVE_ATTRIBUTE()) &&
      (fieldDataInfo->Get(FIELD_ACTIVE_ATTRIBUTE()) & (1 << attributeType )) )
      {
      return fieldDataInfo;
      }
    }
  return NULL;
}

//----------------------------------------------------------------------------
vtkInformation *vtkDataObject::GetNamedFieldInformation(vtkInformation *info, 
                                                        int fieldAssociation, 
                                                        const char *name)
{
  int i;
  vtkInformation *fieldDataInfo;
  vtkInformationVector *fieldDataInfoVector;
  
  if (fieldAssociation == FIELD_ASSOCIATION_POINTS)
    {
    fieldDataInfoVector = info->Get(POINT_DATA_VECTOR());
    }
  else if (fieldAssociation == FIELD_ASSOCIATION_CELLS)
    {
    fieldDataInfoVector = info->Get(CELL_DATA_VECTOR());
    }
  else if (fieldAssociation == FIELD_ASSOCIATION_VERTICES)
    {
    fieldDataInfoVector = info->Get(VERTEX_DATA_VECTOR());
    }
  else if (fieldAssociation == FIELD_ASSOCIATION_EDGES)
    {
    fieldDataInfoVector = info->Get(EDGE_DATA_VECTOR());
    }
  else
    {
    vtkGenericWarningMacro("Unrecognized field association!");
    return NULL;
    }
  
  if (!fieldDataInfoVector)
    {
    return NULL;
    }
  
  for (i = 0; i < fieldDataInfoVector->GetNumberOfInformationObjects(); i++)
    {
    fieldDataInfo = fieldDataInfoVector->GetInformationObject(i);
    if ( fieldDataInfo->Has(FIELD_NAME()) &&
         !strcmp(fieldDataInfo->Get(FIELD_NAME()),name))
      {
      return fieldDataInfo;
      }
    }
  return NULL;
}

//----------------------------------------------------------------------------
void vtkDataObject::RemoveNamedFieldInformation(vtkInformation *info, 
                                                int fieldAssociation, 
                                                const char *name)
{
  int i;
  vtkInformation *fieldDataInfo;
  vtkInformationVector *fieldDataInfoVector;
  
  if (fieldAssociation == FIELD_ASSOCIATION_POINTS)
    {
    fieldDataInfoVector = info->Get(POINT_DATA_VECTOR());
    }
  else if (fieldAssociation == FIELD_ASSOCIATION_CELLS)
    {
    fieldDataInfoVector = info->Get(CELL_DATA_VECTOR());
    }
  else if (fieldAssociation == FIELD_ASSOCIATION_VERTICES)
    {
    fieldDataInfoVector = info->Get(VERTEX_DATA_VECTOR());
    }
  else if (fieldAssociation == FIELD_ASSOCIATION_EDGES)
    {
    fieldDataInfoVector = info->Get(EDGE_DATA_VECTOR());
    }
  else
    {
    vtkGenericWarningMacro("Unrecognized field association!");
    return;
    }
  
  if (!fieldDataInfoVector)
    {
    return;
    }
  
  for (i = 0; i < fieldDataInfoVector->GetNumberOfInformationObjects(); i++)
    {
    fieldDataInfo = fieldDataInfoVector->GetInformationObject(i);
    if ( fieldDataInfo->Has(FIELD_NAME()) &&
         !strcmp(fieldDataInfo->Get(FIELD_NAME()),name))
      {
      fieldDataInfoVector->Remove(fieldDataInfo);
      return;
      }
    }
  return;
}

//----------------------------------------------------------------------------
vtkInformation *vtkDataObject::SetActiveAttribute(vtkInformation *info, 
                                                  int fieldAssociation,
                                                  const char *attributeName,
                                                  int attributeType)
{
  int i;
  vtkInformation *fieldDataInfo;
  vtkInformationVector *fieldDataInfoVector;
  
  if (fieldAssociation == FIELD_ASSOCIATION_POINTS)
    {
    fieldDataInfoVector = info->Get(POINT_DATA_VECTOR());
    }
  else if (fieldAssociation == FIELD_ASSOCIATION_CELLS)
    {
    fieldDataInfoVector = info->Get(CELL_DATA_VECTOR());
    }
  else if (fieldAssociation == FIELD_ASSOCIATION_VERTICES)
    {
    fieldDataInfoVector = info->Get(VERTEX_DATA_VECTOR());
    }
  else if (fieldAssociation == FIELD_ASSOCIATION_EDGES)
    {
    fieldDataInfoVector = info->Get(EDGE_DATA_VECTOR());
    }
  else
    {
    vtkGenericWarningMacro("Unrecognized field association!");
    return NULL;
    }
  if (!fieldDataInfoVector)
    {
    fieldDataInfoVector = vtkInformationVector::New();
    if (fieldAssociation == FIELD_ASSOCIATION_POINTS)
      {
      info->Set(POINT_DATA_VECTOR(), fieldDataInfoVector);
      }
    else if (fieldAssociation == FIELD_ASSOCIATION_CELLS)
      {
      info->Set(CELL_DATA_VECTOR(), fieldDataInfoVector);
      }
    else if (fieldAssociation == FIELD_ASSOCIATION_VERTICES)
      {
      info->Set(VERTEX_DATA_VECTOR(), fieldDataInfoVector);
      }
    else // if (fieldAssociation == FIELD_ASSOCIATION_EDGES)
      {
      info->Set(EDGE_DATA_VECTOR(), fieldDataInfoVector);
      }
    fieldDataInfoVector->FastDelete();
    }

  // if we find a matching field, turn it on (active);  if another field of same
  // attribute type was active, turn it off (not active)
  vtkInformation *activeField = NULL;
  int activeAttribute;
  const char *fieldName;
  for (i = 0; i < fieldDataInfoVector->GetNumberOfInformationObjects(); i++)
    {
    fieldDataInfo = fieldDataInfoVector->GetInformationObject(i);
    activeAttribute = fieldDataInfo->Get( FIELD_ACTIVE_ATTRIBUTE() );
    fieldName = fieldDataInfo->Get(FIELD_NAME());
    // if names match (or both empty... no field name), then set active
    if ( (attributeName && fieldName && !strcmp(attributeName, fieldName)) ||
      (!attributeName && !fieldName) )
      {
      activeAttribute |= 1 << attributeType;
      fieldDataInfo->Set( FIELD_ACTIVE_ATTRIBUTE(), activeAttribute );
      activeField = fieldDataInfo;
      }
    else if ( activeAttribute & (1 << attributeType) )
      {
      activeAttribute &= ~(1 << attributeType);
      fieldDataInfo->Set( FIELD_ACTIVE_ATTRIBUTE(), activeAttribute );
      }
    }
  
  // if we didn't find a matching field, create one
  if (!activeField) 
    {
    activeField = vtkInformation::New();
    activeField->Set( FIELD_ACTIVE_ATTRIBUTE(), 1 << attributeType);
    activeField->Set(FIELD_ASSOCIATION(), fieldAssociation);
    if (attributeName)
      {
      activeField->Set( FIELD_NAME(), attributeName );
      }
    fieldDataInfoVector->Append(activeField);
    activeField->FastDelete();
    }

  return activeField;
}

//----------------------------------------------------------------------------
void vtkDataObject::SetActiveAttributeInfo(vtkInformation *info, 
                                          int fieldAssociation,
                                          int attributeType,
                                          const char *name,
                                          int arrayType,
                                          int numComponents,
                                          int numTuples)
{
  vtkInformation *attrInfo = vtkDataObject::GetActiveFieldInformation(info, 
    fieldAssociation, attributeType);
  if (!attrInfo)
    {
    // create an entry and set it as active
    attrInfo = SetActiveAttribute(info, fieldAssociation, name, attributeType);
    }

  if (name)
    {
    attrInfo->Set(FIELD_NAME(), name);
    }

  // Set the scalar type if it was given.  If it was not given and
  // there is no current scalar type set the default to VTK_DOUBLE.
  if (arrayType != -1)
    {
    attrInfo->Set(FIELD_ARRAY_TYPE(), arrayType);
    }
  else if(!attrInfo->Has(FIELD_ARRAY_TYPE()))
    {
    attrInfo->Set(FIELD_ARRAY_TYPE(), VTK_DOUBLE);
    }

  // Set the number of components if it was given.  If it was not
  // given and there is no current number of components set the
  // default to 1.
  if (numComponents != -1)
    {
    attrInfo->Set(FIELD_NUMBER_OF_COMPONENTS(), numComponents);
    }
  else if(!attrInfo->Has(FIELD_NUMBER_OF_COMPONENTS()))
    {
    attrInfo->Set(FIELD_NUMBER_OF_COMPONENTS(), 1);
    }

  if (numTuples != -1)
    {
    attrInfo->Set(FIELD_NUMBER_OF_TUPLES(), numTuples);
    }
}

//----------------------------------------------------------------------------
void vtkDataObject::SetPointDataActiveScalarInfo(vtkInformation *info,
                                        int arrayType, int numComponents)
  {
  vtkDataObject::SetActiveAttributeInfo(info, FIELD_ASSOCIATION_POINTS, 
    vtkDataSetAttributes::SCALARS, NULL, arrayType, numComponents, -1);
  }

//----------------------------------------------------------------------------
void vtkDataObject::DataHasBeenGenerated()
{
  this->DataReleased = 0;
  this->UpdateTime.Modified();
}

//----------------------------------------------------------------------------
int vtkDataObject::GetGlobalReleaseDataFlag()
{
  return vtkDataObjectGlobalReleaseDataFlag;
}

//----------------------------------------------------------------------------
void vtkDataObject::ReleaseData()
{
  this->Initialize();
  this->DataReleased = 1;
}

//----------------------------------------------------------------------------
void vtkDataObject::Register(vtkObjectBase* o)
{
  this->RegisterInternal(o, 1);
}

//----------------------------------------------------------------------------
void vtkDataObject::UnRegister(vtkObjectBase* o)
{
  this->UnRegisterInternal(o, 1);
}

//----------------------------------------------------------------------------
unsigned long vtkDataObject::GetUpdateTime()
{
  return this->UpdateTime.GetMTime();
}

//----------------------------------------------------------------------------
vtkExecutive* vtkDataObject::GetExecutive()
{
  if(this->PipelineInformation)
    {
    return vtkExecutive::PRODUCER()->GetExecutive(this->PipelineInformation);
    }
  return 0;
}

//----------------------------------------------------------------------------
int vtkDataObject::GetPortNumber()
{
  if(this->PipelineInformation)
    {
    return vtkExecutive::PRODUCER()->GetPort(this->PipelineInformation);
    }
  return 0;
}

//----------------------------------------------------------------------------

unsigned long vtkDataObject::GetActualMemorySize()
{
  return this->FieldData->GetActualMemorySize();
}

//----------------------------------------------------------------------------
void vtkDataObject::ShallowCopy(vtkDataObject *src)
{
  if (!src)
    {
    vtkWarningMacro("Attempted to ShallowCopy from null.");
    return;
    }

  this->InternalDataObjectCopy(src);

  if (!src->FieldData)
    {
    this->SetFieldData(0);
    }
  else
    {
    if (this->FieldData)
      {
      this->FieldData->ShallowCopy(src->FieldData);
      }
    else
      {
      vtkFieldData* fd = vtkFieldData::New();
      fd->ShallowCopy(src->FieldData);
      this->SetFieldData(fd);
      fd->FastDelete();
      }
    }
}

//----------------------------------------------------------------------------
void vtkDataObject::DeepCopy(vtkDataObject *src)
{
  vtkFieldData *srcFieldData = src->GetFieldData();
  
  this->InternalDataObjectCopy(src);

  if (srcFieldData)
    {
    vtkFieldData *newFieldData = vtkFieldData::New();
    newFieldData->DeepCopy(srcFieldData);
    this->SetFieldData(newFieldData);
    newFieldData->FastDelete();
    }
  else
    {
    this->SetFieldData(NULL);
    }
}

//----------------------------------------------------------------------------
void vtkDataObject::InternalDataObjectCopy(vtkDataObject *src)
{
  this->DataReleased = src->DataReleased;
  
  // Do not copy pipeline specific information from data object to
  // data object. This meta-data is specific to the algorithm and the
  // what was requested of it when it executed. What looks like a single
  // piece to an internal algorithm may be a piece to an external
  // algorithm.
  /*
  if(src->Information->Has(DATA_PIECE_NUMBER()))
    {
    this->Information->Set(DATA_PIECE_NUMBER(),
                           src->Information->Get(DATA_PIECE_NUMBER()));
    }
  if(src->Information->Has(DATA_NUMBER_OF_PIECES()))
    {
    this->Information->Set(DATA_NUMBER_OF_PIECES(),
                           src->Information->Get(DATA_NUMBER_OF_PIECES()));
    }
  if(src->Information->Has(DATA_NUMBER_OF_GHOST_LEVELS()))
    {
    this->Information->Set(DATA_NUMBER_OF_GHOST_LEVELS(),
                           src->Information->Get(DATA_NUMBER_OF_GHOST_LEVELS()));
    }
  */
  if(src->Information->Has(DATA_TIME_STEPS()))
    {
    this->Information->CopyEntry(src->Information, DATA_TIME_STEPS(), 1);
    }
  if(src->Information->Has(DATA_RESOLUTION()))
    {
    this->Information->CopyEntry(src->Information, DATA_RESOLUTION(), 1);
    }
  
  // This also caused a pipeline problem.
  // An input pipelineMTime was copied to output.  Pipeline did not execute...
  // We do not copy MTime of object, so why should we copy these.
  //this->PipelineMTime = src->PipelineMTime;
  //this->UpdateTime = src->UpdateTime;
  //this->Locality = src->Locality;
}

//----------------------------------------------------------------------------
// This should be a pure virutal method.
void vtkDataObject::Crop(const int* updateExtent)
{
}

//----------------------------------------------------------------------------
void vtkDataObject::ReportReferences(vtkGarbageCollector* collector)
{
  this->Superclass::ReportReferences(collector);
  vtkGarbageCollectorReport(collector, this->Information, "Information");
  vtkGarbageCollectorReport(collector, this->PipelineInformation,
                            "PipelineInformation");
}


//----------------------------------------------------------------------------
vtkDataObject* vtkDataObject::GetData(vtkInformation* info)
{
  return info? info->Get(DATA_OBJECT()) : 0;
}

//----------------------------------------------------------------------------
vtkDataObject* vtkDataObject::GetData(vtkInformationVector* v, int i)
{
  return vtkDataObject::GetData(v->GetInformationObject(i));
}

//----------------------------------------------------------------------------
const char* vtkDataObject::GetAssociationTypeAsString(int associationType)
{
  if (associationType < 0 || associationType >= NUMBER_OF_ASSOCIATIONS)
    {
    vtkGenericWarningMacro("Bad association type.");
    return NULL;
    }
  return vtkDataObject::AssociationNames[associationType];
}

//----------------------------------------------------------------------------
vtkDataSetAttributes* vtkDataObject::GetAttributes(int type)
{
  return vtkDataSetAttributes::SafeDownCast(this->GetAttributesAsFieldData(type));
}

//----------------------------------------------------------------------------
vtkFieldData* vtkDataObject::GetAttributesAsFieldData(int type)
{
  switch (type)
    {
    case FIELD:
      return this->FieldData;
      break;
    }
  return 0;
}

//----------------------------------------------------------------------------
int vtkDataObject::GetAttributeTypeForArray(vtkAbstractArray* arr)
{
  for (int i = 0; i < NUMBER_OF_ATTRIBUTE_TYPES; ++i)
    {
    vtkFieldData* data = this->GetAttributesAsFieldData(i);
    if (data)
      {
      for (int j = 0; j < data->GetNumberOfArrays(); ++j)
        {
        if (data->GetAbstractArray(j) == arr)
          {
          return i;
          }
        }
      }
    }
  return -1;
}

//----------------------------------------------------------------------------
vtkIdType vtkDataObject::GetNumberOfElements(int type)
{
  switch (type)
    {
    case FIELD:
      return this->FieldData->GetNumberOfTuples();
      break;
    }
  return 0;
}
