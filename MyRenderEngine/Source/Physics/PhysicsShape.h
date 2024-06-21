#pragma once

class IPhysicsShape
{
public:
    virtual ~IPhysicsShape() {};

    virtual bool IsConvexShape() const = 0;
    
    // Only available for convex shapes
    virtual float GetDensity() const = 0;
    virtual void SetDensity() const = 0;   
};