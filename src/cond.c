
// Compiler implementation of the D programming language
// Copyright (c) 1999-2012 by Digital Mars
// All Rights Reserved
// written by Walter Bright
// http://www.digitalmars.com
// License for redistribution is by either the Artistic License
// in artistic.txt, or the GNU General Public License in gnu.txt.
// See the included readme.txt for details.

#include <stdio.h>
#include <assert.h>
#include <string.h>                     // strcmp()

#include "id.h"
#include "init.h"
#include "declaration.h"
#include "identifier.h"
#include "expression.h"
#include "cond.h"
#include "module.h"
#include "template.h"
#include "lexer.h"
#include "mtype.h"
#include "scope.h"
#include "arraytypes.h"

int findCondition(Strings *ids, Identifier *ident)
{
    if (ids)
    {
        for (size_t i = 0; i < ids->dim; i++)
        {
            const char *id = (*ids)[i];

            if (strcmp(id, ident->toChars()) == 0)
                return true;
        }
    }

    return false;
}

/* ============================================================ */

Condition::Condition(Loc loc)
{
    this->loc = loc;
    inc = 0;
}

/* ============================================================ */

DVCondition::DVCondition(Module *mod, unsigned level, Identifier *ident)
        : Condition(Loc())
{
    this->mod = mod;
    this->level = level;
    this->ident = ident;
}

Condition *DVCondition::syntaxCopy()
{
    return this;        // don't need to copy
}

/* ============================================================ */

void DebugCondition::setGlobalLevel(unsigned level)
{
    global.params.debuglevel = level;
}

void DebugCondition::addGlobalIdent(const char *ident)
{
    if (!global.params.debugids)
        global.params.debugids = new Strings();
    global.params.debugids->push((char *)ident);
}


DebugCondition::DebugCondition(Module *mod, unsigned level, Identifier *ident)
    : DVCondition(mod, level, ident)
{
}

// Helper for printing dependency information
void printDepsConditional(Scope *sc, DVCondition* condition, const char* depType)
{
    if (!global.params.moduleDeps || global.params.moduleDepsFile)
        return;
    OutBuffer *ob = global.params.moduleDeps;
    Module* imod = sc ? (sc->instantiatingModule ? sc->instantiatingModule : sc->module) : condition->mod;
    if (!imod)
        return;
    ob->writestring(depType);
    ob->writestring(imod->toPrettyChars());
    ob->writestring(" (");
    escapePath(ob, imod->srcfile->toChars());
    ob->writestring(") : ");
    if (condition->ident)
        ob->printf("%s\n", condition->ident->toChars());
    else
        ob->printf("%d\n", condition->level);
}


int DebugCondition::include(Scope *sc, ScopeDsymbol *s)
{
    //printf("DebugCondition::include() level = %d, debuglevel = %d\n", level, global.params.debuglevel);
    if (inc == 0)
    {
        inc = 2;
        bool definedInModule = false;
        if (ident)
        {
            if (findCondition(mod->debugids, ident))
            {
                inc = 1;
                definedInModule = true;
            }
            else if (findCondition(global.params.debugids, ident))
                inc = 1;
            else
            {   if (!mod->debugidsNot)
                    mod->debugidsNot = new Strings();
                mod->debugidsNot->push(ident->toChars());
            }
        }
        else if (level <= global.params.debuglevel || level <= mod->debuglevel)
            inc = 1;
        if (!definedInModule)
            printDepsConditional(sc, this, "depsDebug ");
    }
    return (inc == 1);
}

void DebugCondition::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    if (ident)
        buf->printf("debug (%s)", ident->toChars());
    else
        buf->printf("debug (%u)", level);
}

/* ============================================================ */

void VersionCondition::setGlobalLevel(unsigned level)
{
    global.params.versionlevel = level;
}

bool VersionCondition::isPredefined(const char *ident)
{
    static const char* reserved[] =
    {
        "DigitalMars",
        "GNU",
        "LDC",
        "SDC",
        "Windows",
        "Win32",
        "Win64",
        "linux",
        "OSX",
        "FreeBSD",
        "OpenBSD",
        "NetBSD",
        "DragonFlyBSD",
        "BSD",
        "Solaris",
        "Posix",
        "AIX",
        "Haiku",
        "SkyOS",
        "SysV3",
        "SysV4",
        "Hurd",
        "Android",
        "Cygwin",
        "MinGW",
        "X86",
        "X86_64",
        "ARM",
        "ARM_Thumb",
        "ARM_SoftFloat",
        "ARM_SoftFP",
        "ARM_HardFloat",
        "AArch64",
        "PPC",
        "PPC_SoftFloat",
        "PPC_HardFloat",
        "PPC64",
        "IA64",
        "MIPS32",
        "MIPS64",
        "MIPS_O32",
        "MIPS_N32",
        "MIPS_O64",
        "MIPS_N64",
        "MIPS_EABI",
        "MIPS_SoftFloat",
        "MIPS_HardFloat",
        "SPARC",
        "SPARC_V8Plus",
        "SPARC_SoftFloat",
        "SPARC_HardFloat",
        "SPARC64",
        "S390",
        "S390X",
        "HPPA",
        "HPPA64",
        "SH",
        "SH64",
        "Alpha",
        "Alpha_SoftFloat",
        "Alpha_HardFloat",
        "LittleEndian",
        "BigEndian",
        "D_Coverage",
        "D_Ddoc",
        "D_InlineAsm_X86",
        "D_InlineAsm_X86_64",
        "D_LP64",
        "D_X32",
        "D_HardFloat",
        "D_SoftFloat",
        "D_PIC",
        "D_SIMD",
        "D_Version2",
        "D_NoBoundsChecks",
        "unittest",
        "assert",
        "all",
        "none",
    };

    for (unsigned i = 0; i < sizeof(reserved) / sizeof(reserved[0]); i++)
    {
        if (strcmp(ident, reserved[i]) == 0)
            return true;
    }

    if (ident[0] == 'D' && ident[1] == '_')
        return true;
    return false;
}

void VersionCondition::addGlobalIdent(const char *ident)
{
    checkPredefined(Loc(), ident);
    addPredefinedGlobalIdent(ident);
}

void VersionCondition::addPredefinedGlobalIdent(const char *ident)
{
    if (!global.params.versionids)
        global.params.versionids = new Strings();
    global.params.versionids->push((char *)ident);
}


VersionCondition::VersionCondition(Module *mod, unsigned level, Identifier *ident)
    : DVCondition(mod, level, ident)
{
}

int VersionCondition::include(Scope *sc, ScopeDsymbol *s)
{
    //printf("VersionCondition::include() level = %d, versionlevel = %d\n", level, global.params.versionlevel);
    //if (ident) printf("\tident = '%s'\n", ident->toChars());
    if (inc == 0)
    {
        inc = 2;
        bool definedInModule=false;
        if (ident)
        {
            if (findCondition(mod->versionids, ident))
            {
                inc = 1;
                definedInModule = true;
            }
            else if (findCondition(global.params.versionids, ident))
                inc = 1;
            else
            {
                if (!mod->versionidsNot)
                    mod->versionidsNot = new Strings();
                mod->versionidsNot->push(ident->toChars());
            }
        }
        else if (level <= global.params.versionlevel || level <= mod->versionlevel)
            inc = 1;
        if (!definedInModule && (!ident || (!isPredefined(ident->toChars()) && ident != Lexer::idPool(Token::toChars(TOKunittest)) && ident != Lexer::idPool(Token::toChars(TOKassert)))))
            printDepsConditional(sc, this, "depsVersion ");
    }
    return (inc == 1);
}

void VersionCondition::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    if (ident)
        buf->printf("version (%s)", ident->toChars());
    else
        buf->printf("version (%u)", level);
}


/**************************** StaticIfCondition *******************************/

StaticIfCondition::StaticIfCondition(Loc loc, Expression *exp)
    : Condition(loc)
{
    this->exp = exp;
    this->nest = 0;
}

Condition *StaticIfCondition::syntaxCopy()
{
    return new StaticIfCondition(loc, exp->syntaxCopy());
}

int StaticIfCondition::include(Scope *sc, ScopeDsymbol *s)
{
#if 0
    printf("StaticIfCondition::include(sc = %p, s = %p) this=%p inc = %d\n", sc, s, this, inc);
    if (s)
    {
        printf("\ts = '%s', kind = %s\n", s->toChars(), s->kind());
    }
#endif
    if (inc == 0)
    {
        if (exp->op == TOKerror || nest > 100)
        {
            error(loc, (nest > 1000) ? "unresolvable circular static if expression"
                                     : "error evaluating static if expression");
            goto Lerror;
        }

        if (!sc)
        {
            error(loc, "static if conditional cannot be at global scope");
            inc = 2;
            return 0;
        }

        ++nest;
        sc = sc->push(sc->scopesym);
        sc->sd = s;                     // s gets any addMember()
        sc->flags |= SCOPEstaticif;

        sc = sc->startCTFE();
        Expression *e = exp->semantic(sc);
        e = resolveProperties(sc, e);
        sc = sc->endCTFE();

        sc->pop();
        --nest;

        if (!e->type->checkBoolean())
        {
            if (e->type->toBasetype() != Type::terror)
                exp->error("expression %s of type %s does not have a boolean value", exp->toChars(), e->type->toChars());
            goto Lerror;
        }
        e = e->ctfeInterpret();
        if (e->op == TOKerror)
        {
            goto Lerror;
        }
        else if (e->isBool(true))
            inc = 1;
        else if (e->isBool(false))
            inc = 2;
        else
        {
            e->error("expression %s is not constant or does not evaluate to a bool", e->toChars());
            goto Lerror;
        }
    }
    return (inc == 1);

Lerror:
    if (!global.gag)
        inc = 2;                // so we don't see the error message again
    return 0;
}

void StaticIfCondition::toCBuffer(OutBuffer *buf, HdrGenState *hgs)
{
    buf->writestring("static if (");
    exp->toCBuffer(buf, hgs);
    buf->writeByte(')');
}
